"""Noyau du moteur : registre de commandes, validation, historique (annuler/rétablir).

Toutes les opérations passent par `Engine.execute(nom, params)`. Blender (bpy) est
la source de vérité : chaque commande agit directement sur bpy.data.

Ce module doit être appelé depuis UN SEUL thread (le thread principal), car bpy
n'est pas thread-safe. Le serveur HTTP sérialise les appels (voir server/executor.py).
"""
from __future__ import annotations

import os
import shutil
import time
import traceback
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

import bpy

from .. import config


class CommandError(Exception):
    """Erreur attendue (paramètre invalide, objet introuvable...) : message pour l'utilisateur."""


class ConfirmationRequired(CommandError):
    """Opération destructrice demandée sans `confirm=true`."""

    def __init__(self, message: str):
        super().__init__(message)


# ---------------------------------------------------------------------------
# Validation des paramètres
# ---------------------------------------------------------------------------
_MISSING = object()


class Params:
    """Accès validé aux paramètres d'une commande."""

    def __init__(self, data: dict | None):
        if data is None:
            data = {}
        if not isinstance(data, dict):
            raise CommandError("Les paramètres doivent être un objet JSON.")
        self.data = data

    def has(self, key: str) -> bool:
        return key in self.data and self.data[key] is not None

    def raw(self, key: str, default: Any = None) -> Any:
        v = self.data.get(key)
        return default if v is None else v

    def str(self, key: str, default: Any = _MISSING, choices: list[str] | None = None, max_len: int = 4000) -> str:
        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        if not isinstance(v, str):
            v = str(v)
        if len(v) > max_len:
            raise CommandError(f"« {key} » est trop long ({len(v)} > {max_len}).")
        if choices is not None:
            low = {c.lower(): c for c in choices}
            if v.lower() not in low:
                raise CommandError(f"« {key} » doit être l'une des valeurs : {', '.join(choices)} (reçu : {v}).")
            v = low[v.lower()]
        return v

    def float(self, key: str, default: Any = _MISSING, lo: float | None = None, hi: float | None = None) -> float:
        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        try:
            f = float(v)
        except (TypeError, ValueError):
            raise CommandError(f"« {key} » doit être un nombre (reçu : {v!r}).") from None
        if f != f:  # NaN
            raise CommandError(f"« {key} » n'est pas un nombre valide.")
        if lo is not None and f < lo:
            raise CommandError(f"« {key} » doit être ≥ {lo} (reçu : {f}).")
        if hi is not None and f > hi:
            raise CommandError(f"« {key} » doit être ≤ {hi} (reçu : {f}).")
        return f

    def int(self, key: str, default: Any = _MISSING, lo: int | None = None, hi: int | None = None) -> int:
        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        try:
            if isinstance(v, bool):
                raise ValueError
            f = float(v)
            if f != int(f):
                raise ValueError
            i = int(f)
        except (TypeError, ValueError):
            raise CommandError(f"« {key} » doit être un entier (reçu : {v!r}).") from None
        if lo is not None and i < lo:
            raise CommandError(f"« {key} » doit être ≥ {lo} (reçu : {i}).")
        if hi is not None and i > hi:
            raise CommandError(f"« {key} » doit être ≤ {hi} (reçu : {i}).")
        return i

    def bool(self, key: str, default: Any = _MISSING) -> bool:
        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        if isinstance(v, bool):
            return v
        if isinstance(v, (int, float)):
            return bool(v)
        if isinstance(v, str) and v.lower() in ("true", "1", "oui", "yes", "on"):
            return True
        if isinstance(v, str) and v.lower() in ("false", "0", "non", "no", "off"):
            return False
        raise CommandError(f"« {key} » doit être vrai/faux (reçu : {v!r}).")

    def vec(self, key: str, default: Any = _MISSING, size: int = 3) -> tuple:
        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            v = [v] * size
        if not isinstance(v, (list, tuple)) or len(v) != size:
            raise CommandError(f"« {key} » doit être une liste de {size} nombres.")
        try:
            out = tuple(float(x) for x in v)
        except (TypeError, ValueError):
            raise CommandError(f"« {key} » doit contenir uniquement des nombres.") from None
        if any(x != x or abs(x) > 1e7 for x in out):
            raise CommandError(f"« {key} » contient une valeur invalide.")
        return out

    def color(self, key: str, default: Any = _MISSING) -> tuple:
        """Couleur RVB(A) 0..1, ou '#rrggbb', ou nom de couleur connu."""
        from .colors import parse_color

        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        c = parse_color(v)
        if c is None:
            raise CommandError(f"« {key} » : couleur non reconnue ({v!r}). Utilisez '#rrggbb', [r,g,b] ou un nom.")
        return c

    def list(self, key: str, default: Any = _MISSING) -> list:
        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        if isinstance(v, str):
            return [v]
        if not isinstance(v, (list, tuple)):
            raise CommandError(f"« {key} » doit être une liste.")
        return list(v)

    def dict(self, key: str, default: Any = _MISSING) -> dict:
        v = self.data.get(key)
        if v is None:
            if default is _MISSING:
                raise CommandError(f"Paramètre obligatoire manquant : « {key} ».")
            return default
        if not isinstance(v, dict):
            raise CommandError(f"« {key} » doit être un objet.")
        return v

    def confirm(self, what: str) -> None:
        if not self.bool("confirm", False):
            raise ConfirmationRequired(
                f"Opération destructrice : {what}. Relancez avec confirm=true pour confirmer."
            )


# ---------------------------------------------------------------------------
# Registre
# ---------------------------------------------------------------------------
@dataclass
class Command:
    name: str
    fn: Callable[["Engine", Params], dict]
    label: str
    history: bool  # crée un point d'annulation
    changes_scene: bool
    description: str = ""
    destructive: bool = False


COMMANDS: dict[str, Command] = {}


def command(name: str, label: str | None = None, history: bool = True, changes_scene: bool | None = None,
            destructive: bool = False):
    def deco(fn):
        COMMANDS[name] = Command(
            name=name,
            fn=fn,
            label=label or name,
            history=history,
            changes_scene=history if changes_scene is None else changes_scene,
            description=(fn.__doc__ or "").strip(),
            destructive=destructive,
        )
        return fn

    return deco


# ---------------------------------------------------------------------------
# Historique par instantanés .blend
# ---------------------------------------------------------------------------
@dataclass
class HistoryEntry:
    label: str
    path: str
    time: float = field(default_factory=time.time)


class History:
    """Annuler/rétablir fiable : chaque étape est un instantané .blend complet.

    En mode sans interface (headless), l'annulation native de Blender n'est pas
    disponible ; les instantanés offrent un comportement exact et vérifiable.
    """

    def __init__(self, folder: Path, limit: int = 40):
        self.folder = folder
        self.limit = max(5, limit)
        self.entries: list[HistoryEntry] = []
        self.index = -1
        self._counter = 0
        if folder.exists():
            shutil.rmtree(folder, ignore_errors=True)
        folder.mkdir(parents=True, exist_ok=True)

    def _snapshot(self) -> str:
        self._counter += 1
        path = str(self.folder / f"step_{self._counter:05d}.blend")
        bpy.ops.wm.save_as_mainfile(filepath=path, copy=True, compress=False, check_existing=False)
        return path

    def reset(self, label: str = "Départ") -> None:
        for e in self.entries:
            _silent_remove(e.path)
        self.entries = [HistoryEntry(label, self._snapshot())]
        self.index = 0

    def push(self, label: str) -> None:
        # coupe la branche « rétablir »
        for e in self.entries[self.index + 1:]:
            _silent_remove(e.path)
        self.entries = self.entries[: self.index + 1]
        self.entries.append(HistoryEntry(label, self._snapshot()))
        while len(self.entries) > self.limit:
            old = self.entries.pop(0)
            _silent_remove(old.path)
        self.index = len(self.entries) - 1

    def can_undo(self) -> bool:
        return self.index > 0

    def can_redo(self) -> bool:
        return self.index < len(self.entries) - 1

    def goto(self, index: int) -> HistoryEntry:
        if not 0 <= index < len(self.entries):
            raise CommandError("Étape d'historique inexistante.")
        self.index = index
        entry = self.entries[index]
        bpy.ops.wm.open_mainfile(filepath=entry.path, load_ui=False)
        return entry

    def as_list(self) -> list[dict]:
        return [
            {"index": i, "label": e.label, "time": e.time, "current": i == self.index}
            for i, e in enumerate(self.entries)
        ]


def _silent_remove(path: str) -> None:
    for p in (path, path + "1"):
        try:
            os.remove(p)
        except OSError:
            pass


# ---------------------------------------------------------------------------
# Moteur
# ---------------------------------------------------------------------------
class Engine:
    def __init__(self, settings: dict | None = None, listener: Callable[[dict], None] | None = None):
        self.settings = settings or config.load_settings()
        self.home = config.home()
        self.listener = listener  # reçoit les événements (progression, scène modifiée...)
        self.defer = None  # file d'exécution différée (fournie par le serveur)
        self.version = 0
        self.project_path: str | None = None
        self.dirty = False
        self.log: list[dict] = []
        # charge tous les modules qui enregistrent des commandes
        from . import register_all

        register_all()
        bpy.ops.wm.read_factory_settings(use_empty=True)
        from .scene_ops import setup_default_scene

        setup_default_scene()
        self.history = History(self.home / "history" / str(os.getpid()), self.settings.get("history_limit", 40))
        self.history.reset("Nouvelle scène")

    # -- événements -------------------------------------------------------
    def emit(self, event: dict) -> None:
        if self.listener:
            try:
                self.listener(event)
            except Exception:  # un auditeur défaillant ne doit pas casser une opération
                traceback.print_exc()

    def progress(self, message: str, fraction: float | None = None) -> None:
        self.emit({"type": "progress", "message": message, "fraction": fraction})

    def info(self, message: str, level: str = "info") -> None:
        entry = {"time": time.time(), "level": level, "message": message}
        self.log.append(entry)
        del self.log[:-500]
        self.emit({"type": "log", **entry})

    # -- exécution --------------------------------------------------------
    def execute(self, name: str, params: dict | None = None) -> dict:
        cmd = COMMANDS.get(name)
        if cmd is None:
            raise CommandError(f"Commande inconnue : « {name} ».")
        p = Params(params)
        t0 = time.time()
        try:
            result = cmd.fn(self, p)
        except CommandError:
            raise
        except RuntimeError as e:  # erreurs des opérateurs Blender
            msg = str(e).strip().splitlines()[-1] if str(e).strip() else repr(e)
            raise CommandError(f"Blender a refusé l'opération : {msg}") from e
        if result is None:
            result = {}
        if cmd.changes_scene:
            self.version += 1
            self.dirty = True
        if cmd.history:
            label = result.pop("_label", None) or cmd.label
            self.history.push(label)
            self.emit({"type": "history", "history": self.history.as_list()})
        if cmd.changes_scene:
            self.emit({"type": "scene_changed", "version": self.version, "command": name})
        elapsed = time.time() - t0
        if cmd.history:
            self.info(f"{cmd.label} ({elapsed:.2f} s)")
        return result

    # -- annuler / rétablir ----------------------------------------------
    def undo(self) -> dict:
        if not self.history.can_undo():
            raise CommandError("Rien à annuler.")
        undone = self.history.entries[self.history.index].label
        self.history.goto(self.history.index - 1)
        return self._after_history_move(f"Annulé : {undone}")

    def redo(self) -> dict:
        if not self.history.can_redo():
            raise CommandError("Rien à rétablir.")
        e = self.history.goto(self.history.index + 1)
        return self._after_history_move(f"Rétabli : {e.label}")

    def goto_history(self, index: int) -> dict:
        e = self.history.goto(index)
        return self._after_history_move(f"Retour à : {e.label}")

    def _after_history_move(self, message: str) -> dict:
        self.version += 1
        self.dirty = True
        self.info(message)
        self.emit({"type": "history", "history": self.history.as_list()})
        self.emit({"type": "scene_changed", "version": self.version, "command": "history"})
        return {"message": message, "history": self.history.as_list()}

    def status(self) -> dict:
        return {
            "version": self.version,
            "project_path": self.project_path,
            "dirty": self.dirty,
            "blender_version": bpy.app.version_string,
            "can_undo": self.history.can_undo(),
            "can_redo": self.history.can_redo(),
        }
