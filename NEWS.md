# Changes

## v2.1 (released ????/??/??)

## v2.0.1 - Windows UI Bugfix (released 2025/06/17)

* Bug fixes:
  * Fix Windows UI issues: missing icons, proper Dark Mode support.

## v2.0 - GUI Release (released 2025/06/16)

* New features:
  * GUI frontend written using the Qt Toolkit.
    * Qt5 and Qt6 are supported.
    * The precompiled Windows build uses a patched version of
      Qt 6.8.3 that supports Windows 7.
  * [Win32] Implemented the `query` command.
  * wadresign: Command line tool to recrypt and resign WAD files.
    * Can decrypt WADs using any key: retail, Korean, debug
    * Can encrypt WADs using any key. The resulting WAD will be fakesigned if
      encrypting for retail or Korean, or realsigned if encrypting for debug.
    * Both standard and BroadOn WAD format are supported for both input and
      output.
    * **WARNING:** Use with caution if converting system titles for use
      on real hardware.
  * nusresign: Command line tool to recrypt and resign NUS/WUP packages.
    * Can decrypt and encrypt from and to retail and debug.
    * The resulting NUS package will be fakesigned if encrypting for retail,
      or realsigned if encrypting for debug.
    * **WARNING:** Use with caution if converting system titles for use
      on real hardware.

* Low-level changes:
  * Rewrote librvth using C++ to improve maintainability.

* Other changes:
  * Realsigned tickets and TMDs are now explicitly indicated as such.

* Bug fixes:
  * Dual-layer images weren't imported properly before. Debug builds asserted
    at 4489 MB, but release builds silently failed.

## v1.1.1 - Brown Paper Bag Release (released 2018/09/17)

* Extracting images broke because it failed when opening a file with 0 bytes
  length, which always happened when creating new files.

## v1.1 - More Bug Fixes (released 2018/09/17)

* [Win32] Fixed obtaining the device size. fseek(SEEK_END) worked fine on
  some systems, but not on others.
* Fixed an incorrect "Success" error message in some cases.
* Use a fake NHCD table if a real NHCD header can't be found. This should
  allow recovery of banks on some systems that have a blanked-out NHCD table.

## v1.0 - Bug Fixes and Recryption (released 2018/06/07)

* Fixed extracting unencrypted images from RVT-H Reader devices. This broke
  when disabling the recryption functionality close to v0.9's release.
* Fixed an issue that broke recryption from unencrypted debug-signed to
  encrypted debug-signed.
* Enabled recryption. The issue I was having with "The Last Story" was caused
  by main.dol being slightly out of spec, which is allowed on devkits but not
  on retail. An upcoming Dolphin patch will fix this as well.
* Added an AppLoader DOL verifier to check for known issues with DOL files in
  disc images. This is currently only supported for GameCube and unencrypted
  Wii disc images.

## v0.9 - Initial Release (released 2018/04/26)

* Lists all disc images currently installed on the RVT-H system.
* Can find "deleted" images that aren't accessible on the RVT-H but are still
  present on the HDD, and has an option to undelete these images.
  * Can undelete any image deleted by rvtwriter, which only clears the
    bank table entry.
  * Can undelete Wii images that were "flushed" using the front-panel button.
    * GameCube images deleted this way cannot currently be undeleted.
* Extracts disc images from an RVT-H system or HDD image into a GCM file.
  * Supports optional recryption to convert a debug-encrypted image to retail
    encryption with fake-signed ticket and TMD.
* Signature verification for all Wii disc images. Indicates if the signature
  is valid, invalid, or fakesigned. (Game partition only at the moment.)
* Installation of GameCube and Wii disc images onto an RVT-H system. If
  importing a retail Wii disc image, it will automatically be re-signed and
  re-encrypted using the debug keys. (Update partitions will be removed, since
  retail updates won't work properly on RVT-H.)
  * Supported image formats: GCM, headered GCM, CISO, WBFS
  * Split WBFS is not currently supported. Combine the .wbfs and .wbfs1 files
    before processing.
* Standalone disc image re-signing to convert e.g. retail to debug, debug
  to retail, unencrypted to debug, etc. Conversion to retail will result
  in a fakesigned image.
* Querying all available RVT-H Reader devices to determine which device name
  is associated with which reader. (Linux with UDEV only at the moment.)

NOTE: Converting an unencrypted image to encrypted is not currently supported.
