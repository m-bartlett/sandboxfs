
<table>
  <th>
    <h3>&nbsp;&nbsp;sandboxFS&nbsp;&nbsp;</h3>
    <img align="center"  width="75" alt="sandboxfs logo" src="https://github.com/user-attachments/assets/413071d5-6dc4-432a-90f1-4ff842ccedab" />
    <br/>
    <br/>
  </th>
<td>
<p>
          
Create a replica of the current machine's root filesystem with an overlay mount that isolates filesystem mutations. The goal of this tool is to help users maintain a tidy file system. See [use cases](use-cases) for examples of how this tool can be used effectively.
        
</p>
</td>
</table>
<br/>

## About

> [!IMPORTANT]
> This tool is **not** designed to be used for security. The primary use for this application is to help maintain filesystem cleaniness.
> It does not focus on or prioritize effectively "jailing" applications. A purpose-built tool like [firejail](https://firejail.wordpress.com) should be used for such purposes.
<br/>

`sandboxfs` creates simplistic "containers" and works similarly to the likes of [arch-chroot](https://man.archlinux.org/man/arch-chroot.8) and [bubblewrap](https://github.com/containers/bubblewrap). The main difference is that its containers simulate running on the actual system but isolate filesystem changes.

The container is essentially constructed with:

* Unshares process mount namespace, so mounts are not visible to the broader system
* Creates overlay mount with the filesystem root (`/`) as the "lower directory".
* Creates pseudofs mounts within the overlay (e.g. /sys/, /dev/, /proc/, etc.) that most utilities expect to exist.
* Pivot the process's root to be the overlay mount point

This setup makes the child process use the overlay as its root, meaning we can run any utility with the current filesystem state. However, any filesystem changes trigger the "copy on write" mechanism of overlay mounts meaning the modified files are copied to the overlay mount's "work directory" and the edits are made to the copy, which is then substituted for the base file for all future file operations.

## Build & Install
```sh
cmake -S . -B ./build --fresh
cmake --build ./build --target all -j$(( $(nproc) - 1 ))
sudo cmake --install ./build
```

The resulting binary relies on being owned by the root user with the setuid bit enabled to allow non-root users to perform the necessary mount operations that requires elevated privileges to perform. Once the sandbox environment is created, the root privileges are shed and the resulting process runs as the user.


## Use cases

### "Try before you install"
The primary use case motivating this tool was mitigating so-called "bloat", like the scenario where a user needs to install supplemental OS packages to compile software. A sandboxfs running a shell can be used to install the OS packages, compile the software, and copy the resulting binaries out of sandbox and then exist the sandboxfs. Any installed packages not necessary after building the software will be automatically removed (or more specifically, they were never legitimately installed in the system) including any cache files or other unused byproducts during the compilation process.

It can also be desirable to try many pieces of software out for a particular need. Rather than having to read each tool's documentation (or lackthereof) on where they store use configuration or other state/cache data and cleaning it up manually, you can run the software within a sandboxfs. If the software does not meet your expectations, you can terminate the sandboxfs and be certain that any configuration, state, cache, and other files are cleaned up and not lingering in your system.

### Isolate "messy" software
A sandboxfs instance with a non-ephemeral source directory (i.e. create a directory and supply the path to the `--source` flag) can create a reusable environment that tracks *only* what files have changed. This can be desirable for applications that proliferate across the filesystem and don't supply an AppImage or other means of minimizing filesystem sprawl. One example I use this approach for is the Steam desktop application. When I install games and make other configuration, I can be certain that any related files are stored under `~/Games/Steam/sandboxfs`. This has the added benefit of making this sandbox semi-portable, as the contents can be copied to another system (with the same architecture, your milage may vary) and the installed libraries and game contents will overlay into their expected locations. The portability here is a happy coincidence and not something I would recommend relying on.


<!-- This creates a replica of the root for any file read operations, and any file write operations trigger the "copy-on-write" mechanism that copies the file to a staging directory and writes the changes to the copied file. Files in the staging directory *overlay* (i.e. mask) files on the rootfs and any future reads return the modified copy's contents. This effectively means that filesystem mutations can be isolated to the staging directory. -->
