Building A Stand-alone Binary
=============================

This document describes how to build a completely self-contained,
statically linked "althttpd" binary for
Linux (or similar unix-like operating systems).

What You Need
-------------

  1.  The `althttpd.c` source code file from this website.
  2.  A tarball of the latest [OpenSSL](https://openssl.org) release.
  3.  The usual unix build tools, "make", "gcc", and friends.
  4.  The OpenSSL build requires Perl.

Build Steps
-----------

<ol type="1">
<a name="openssl"></a>
<li><b>Compiling a static OpenSSL library</b>.
<p>
<ul>
<li> Unpack the OpenSSL tarball.  Rename the top-level directory to "openssl".
<li> CD into the openssl directory.
<li> Run these commands:
<blockquote><pre>
./config no-ssl3 no-weak-ssl-ciphers no-shared no-threads no-tls-deprecated-ec --openssldir=/usr/lib/ssl
make CFLAGS=-Os
</pre></blockquote>
<li> Fix a cup of tea while OpenSSL builds....  Note that the
     `no-tls-deprecated-ec` option is only available with OpenSSL-3.5.0
     and later, so omit that option for earlier versions.
</ul>

<p>
<a name="buildapp"></a>
<li><b>Compiling `althttpd`</b>
<p>
<ul>
<li> CD back up into the top-level directory where the `althttpd.c`
     source file lives.
<li> Run:
<blockquote><pre>
make VERSION.h
gcc -I./openssl/include -Os -Wall -Wextra -DENABLE_TLS &#92;
  -o althttpd althttpd.c -L./openssl -lssl -lcrypto -ldl
</pre></blockquote>
<li> The commands in the previous bullet builds a binary that is statically
     linked against OpenSSL, but is still dynamically linked against
     system libraries.
<li> The "make VERSION.h" step above constructs a generated file named
     "VERSION.h" that contains the specific version number of althttpd
     that you are building.
</ul>

<p>
<a name="fullstatic"></a>
<li><b>Completely Static Build</b>
<p>

<ul>
<li> Sometimes it is necessary to do a full static build, in order to
     move the binary to systems with older or incompatible C-libraries.
<li> Note that with a static build using GLibC, the getpwnam() interface
     will not work, and hence the "--user" option to althttpd won't work.
     When invoking a static althttpd build, avoid using the --user option.
     Set the owner of the --root directory to the desired user instead.
     Both the owner and group-owner of the --root diretory must be
     non-zero, something other than root.
<li> Run:
<blockquote><pre>
make VERSION.h
cc -static -I$OPENSSLDIR/include -Os -Wall -Wextra -DENABLE_TLS &#92;
  -o althttpd althttpd.c $OPENSSLDIR/\*.a $OPENSSLDIR/\*.a
</pre></blockquote>
<li> In the previous $OPENSSLDIR resolves to the name of the openssl
     build directory.  For example: "$HOME/openssl/openssl-3.5.5".
<li> Yes, it really is necessary to do the "$OPENSSLDIR/\*.a" twice.
     That is not a typo.
<li> Verify using commands like this:
<blockquote><pre>
./althttpd --version
ldd ./althttpd
./althttpd --popup
</pre></blockquote>
</ul>
</ol>



Using The Binary
----------------

The above is all you need to do.  After compiling, simply move the
resulting binary into the /usr/bin directory of your server.
