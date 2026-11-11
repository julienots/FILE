import os
import sys
import tempfile

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
os.environ.setdefault("SUPERESSENCE_HOME", tempfile.mkdtemp(prefix="se3d_test_"))


@pytest.fixture(scope="session")
def engine():
    from superessence.engine.core import Engine
    events = []
    e = Engine(listener=events.append)
    e._events = events
    return e


@pytest.fixture()
def fresh(engine):
    engine.execute("new_project", {"confirm": True})
    return engine
