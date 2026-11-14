# Parked games. Same contract and same auto-discovery as games/<id>/, but the hub menu does NOT list them:
# they are built, unit-tested and smoke-tested, and the hub reaches them through its archive (see docs/DEFERRED.md).
# To defer a game:  move games/<id> to deferred/<id>.   To bring it back: move it the other way and re-run CMake.
arcade_discover_games()
