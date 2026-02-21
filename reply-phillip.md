# Phillip LeMasters — Support Thread

## Email 1: Download fix (sent)

Hi Phillip,

Thanks for reporting the download issue. The root cause was a mismatch between the filename declared in pak.json and the actual release asset on GitHub. This has been fixed.

To update via Pak Store:

1. Open Pak Store and search for Mono
2. The download should now work directly (v1.9.2)

If Pak Store still shows the old version (cache), install manually:

1. Download "mono-release.zip" from https://github.com/Berckan/mono/releases/tag/v1.9.2
2. Extract the contents to /Tools/tg5040/Mono.pak/ on your SD card (replace existing files)
3. Reboot

Let me know if you run into anything else.

## Email 2: Boot issue — asking for info (sent 2026-02-20)

Hi Phillip,

Glad the download is working now. The fact that it installs but doesn't run on boot points to a permissions issue: the Trimui needs `launch.sh` and the binary to be marked as executable, and some extraction methods strip that flag.

A couple of quick questions so I can point you in the right direction:

1. **What firmware are you running?** (NextUI, MinUI, stock, etc.)
2. **Do you have SSH or Telnet access to your Brick?** If so, this is a one-command fix.
3. **Are you on Windows or Mac?** This matters because Windows doesn't preserve Linux file permissions when extracting zips, so Pak Store installs from a Windows-formatted SD card can lose them.

Once I know your setup I can give you exact steps.
