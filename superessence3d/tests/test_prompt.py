"""Analyseur de descriptions (Python pur)."""
import importlib.util
import os
import sys
import types

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


@pytest.fixture(scope="module")
def prompt():
    # import sans bpy : on charge les modules purs directement
    pkg = types.ModuleType("se_pure")
    pkg.__path__ = []
    sys.modules.setdefault("se_pure", pkg)
    spec_c = importlib.util.spec_from_file_location("se_pure.colors", os.path.join(ROOT, "superessence/engine/colors.py"))
    colors = importlib.util.module_from_spec(spec_c)
    spec_c.loader.exec_module(colors)
    sys.modules["se_pure.colors"] = colors
    src = open(os.path.join(ROOT, "superessence/engine/generators/prompt.py"), encoding="utf-8").read()
    src = src.replace("from ..colors import", "from se_pure.colors import")
    mod = types.ModuleType("se_pure.prompt")
    exec(compile(src, "prompt.py", "exec"), mod.__dict__)
    return mod


def test_dragon(prompt):
    s = prompt.parse_prompt("Crée un dragon noir de 2 mètres, avec quatre ailes, des cornes, des écailles détaillées, "
                            "des yeux rouges et une queue avec des pointes.")
    assert s["category"] == "creature" and s["subtype"] == "dragon"
    assert s["height"] == 2.0
    assert s["features"]["wings"] == 4
    assert s["features"]["horns"] == 2
    assert s["features"]["spikes"] is True and s["features"]["scales"] is True
    assert s["colors"]["primary"][0] < 0.1
    assert s["colors"]["eyes"][0] > 0.5 and s["colors"]["eyes"][1] < 0.2


def test_modification(prompt):
    s = prompt.parse_prompt("un dragon avec deux ailes")
    n, changes = prompt.parse_modification("Agrandis ses ailes, ajoute six tentacules et remplace les yeux par des "
                                           "yeux blancs lumineux.", s)
    assert n["scales"]["wings"] > 1.0
    assert n["features"]["tentacles"] == 6
    assert n["colors"]["eyes"][0] > 0.8 and n["glow_eyes"] is True
    assert len(changes) >= 3


@pytest.mark.parametrize("text,cat,sub", [
    ("un château médiéval", "building", "castle"), ("a red sports car", "vehicle", "car"),
    ("une épée en acier", "weapon", "sword"), ("un loup gris", "animal", "wolf"), ("un chevalier", "character", "humanoid"),
    ("une forêt dense", "scenery", "forest"), ("un tonneau", "prop", "barrel"), ("un vaisseau spatial", "vehicle", "spaceship"),
    ("un kraken géant", "creature", "kraken"), ("une araignée", "animal", "spider"),
])
def test_categories(prompt, text, cat, sub):
    s = prompt.parse_prompt(text)
    assert (s["category"], s["subtype"]) == (cat, sub)


def test_material_text(prompt):
    m = prompt.parse_material_text("Crée une peau de monstre noire, humide, avec des veines violettes et des reliefs organiques.")
    assert m["preset"] == "organic"
    assert m["params"]["wet"] > 0 and m["params"]["bump"] > 1
    assert m["params"]["color"][0] < 0.1
    assert "veins" in m["params"] and m["params"]["veins"][2] > m["params"]["veins"][1]
