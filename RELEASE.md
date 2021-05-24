GUsb Release Notes
==================

Write `NEWS` entries for GUsb in the same format as usual.

    git shortlog 0.3.7.. | grep -i -v trivial | grep -v Merge > NEWS.new
    =================================================
    Version 0.3.8
    ~~~~~~~~~~~~~
    Released: 2021-xx-xx

    Notes:

    New Features:

    Bugfixes:
    =================================================

Update library version if new ABI or API in `meson.build`, commit, and build tarball:

    # MAKE SURE THIS IS CORRECT
    export release_ver="0.3.8"

    git commit -a -m "Release version ${release_ver}"
    git tag -s -f -m "Release ${release_ver}" "${release_ver}"
    <gpg password>
    ninja dist
    git push --tags
    git push
    gpg -b -a meson-dist/libgusb-${release_ver}.tar.xz

Upload tarball:

    scp meson-dist/libgusb-${release_ver}.tar.* hughsient@people.freedesktop.org:public_html/releases/

Do post release version bump in `meson.build` and commit changes:

    git commit -a -m "trivial: post release version bump"
    git push

Send an email to devkit-devel@lists.freedesktop.org

    =================================================
    GUsb 0.3.8 released!

    GUsb is a GObject wrapper for libusb1 that makes it easy to do
    asynchronous control, bulk and interrupt transfers with proper
    cancellation and integration into a mainloop.

    Tarballs available here: http://people.freedesktop.org/~hughsient/releases/
    =================================================
