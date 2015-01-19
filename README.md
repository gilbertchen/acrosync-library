# acrosync-library
[Acrosync] is a new cross-platform rsync client for Windows/Max/iOS/Android that we built from scratch, without taking any code from the open source rsync project.  

This is the library behind [Acrosync], which implements a large portion of the client-side of the (undocumented) rsync protocol, including the famous delta sync algorithm.

[PhotoBackup], our iOS app for uploading photos and videos to computers, uses a slightly modified version of this library.

##Features
- Talks the rsync protocol version 29 (rsync 2.6.4+) and version 30 (rsync 3.x.x). 
- Written in C++ and builds on Win32, Mac OS X, Linux, iOS, and Android.
- The only dependencies are libssh2 and openssl.
- Can connect to the rsync server either via ssh, or via the rsync daemon protocol.
- For ssh connections, supports both password login and public key authentication (with or without a passphrase).
- Symbolic links are supported.

## Build Instructions

First you'll need to install openssl and libssh2.  Assume we're on a linux machine and these two packages are already installed in the default locations.

Run the following command to build the test programs:
```sh
$ python qi/qi-make.py test
```
You will find a new executable *build-linux/rsync/t_rsync_client* which can download a remote directory using the rsync over ssh protocol:

```sh
$ build-linux/rsync/t_rsync_client <server> <username> <password> <remote dir> <local dir>
```
The same build command works for Mac and Windows, but you will need to install openssl and libssh2 to a subdirectory named *install-mac* or *install-win* under the top level directory.

Here the build system is written with [Qi-Make], a tool that we developed by extending the basic syntax of make.  The file *qi/qi_build.h* contains actual rules for building intermediate objects and final test programs.  It should be fairly easy to make changes for your own build environments.

##License

This library is licensed under the Reciprocal Public License.  If this license does not work for you, a commercial license is available for a one-time fee or on a subscription basis.  Contact <software2015@acrosync.com> for licensing details.

[acrosync]:https://acrosync.com
[PhotoBackup]:https://acrosync.com/photobackup.html
[Qi-Make]:https://code.google.com/p/qi-make/
