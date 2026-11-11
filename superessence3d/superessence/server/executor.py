"""File d'exécution sur le thread principal : bpy n'accepte qu'un seul thread."""
from __future__ import annotations

import queue
import threading
import traceback
from concurrent.futures import Future


class MainThreadExecutor:
    def __init__(self):
        self._q: queue.Queue = queue.Queue()
        self._stop = threading.Event()
        self.busy_label: str | None = None

    def submit(self, fn, *args, label: str | None = None, **kwargs) -> Future:
        fut: Future = Future()
        self._q.put((fut, fn, args, kwargs, label))
        return fut

    def stop(self):
        self._stop.set()
        self._q.put(None)

    def run_forever(self):
        while not self._stop.is_set():
            item = self._q.get()
            if item is None:
                continue
            fut, fn, args, kwargs, label = item
            if not fut.set_running_or_notify_cancel():
                continue
            self.busy_label = label
            try:
                fut.set_result(fn(*args, **kwargs))
            except BaseException as e:  # transmis à l'appelant
                if not isinstance(e, Exception):
                    traceback.print_exc()
                fut.set_exception(e)
            finally:
                self.busy_label = None
