# 1.0.0-beta.7
- Use events for logs and exiting

# 1.0.0-beta.6
- 2.2081 support
- Fix console showing after crash

# 1.0.0-beta.5
- Exports GTK_USE_PORTAL=1 to open the system file portal
- Adds custom console color options
- Fixes console not closing on Geode Restart
- Uses unique temp directory to prevent conflict with multiple instances of GD
- General bug fixes and polish

# 1.0.0-beta.4
- Remove potentially unsafe std::atexit
- Keep exit heartbeat on a separate thread to allow it to work if the game hangs

# 1.0.0-beta.3
- Remove const_cast, and copy instead

# 1.0.0-beta.2
- Close console if game crashes
- Add missing "All Files" filter

# 1.0.0-beta.1
- Initial Release
