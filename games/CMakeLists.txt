# Auto-discovery: every folder games/<id>/ that contains a CMakeLists.txt is a game, and the hub menu lists it.
# Folders starting with "_" (e.g. _template) are ignored. Games parked in ../deferred are discovered the same way.
arcade_discover_games()
