Stand-alone Operation
=====================

Suppose you have a directory named ~/www/default.website that contains
all the files to implement a website.  These can be static files or CGI
if the execute bit is set.  You can run Althttpd to serve that website
as follows:

> ~~~
althttpd -root ~/www -port 8080
~~~

The "root ~/www" tells althttpd where to go to find the files for
the website.  The "-port 8080" option is what tells althttpd to run
in stand-alone mode, listening on port 8080.

Stand-alone with HTTPS
----------------------

If althttpd is built with TLS support then it can be told to operate
in HTTPS mode with one of the following options:

> ~~~
althttpd -root ~/www --port 8043 --cert unsafe-builtin
~~~

this option uses a compiled-in self-signed SSL certificate
**which is wildly insecure** and is intended for testing purposes
only.  Use the --cert option to specify your own PEM-format SSL
certificate.  The argument to --cert can be the concatenation of
the SSL private key (often named "privkey.pem") and the certificate
chain (often named "fullchain.pem").  Alternatively, the --cert
can point to just the fullchain.pem file and the separate --pkey
option can point to the privkey.pem file.  Or, more conveniently,
you can use the --certdir option to specify the name of a directory
that contains both the "fullchange.pem" and "privkey.pem" files.

If you are setting up a new website and you do not have a cert yet,
but hope to get one using [Let's Encrypt](https://letsencrypt.org/) or
similar, you can launch your website as follows:

> ~~~
althttpd -root ~/www -port 80 -tls-port 443 \
  -certdir /etc/letsencrypt/live/main
~~~

Because you do not yet have a cert, this will initially only launch
althttpd listening on port 80.  But after you get a cert, you can
relaunch exactly the same command and then it will listen on both 80
(unencrypted) and on port 443 (encrypted).

The command above assumes that your cert files will eventually be
placed in the directory called "/etc/letsencrypt/live/main".  You can
achieve this by including the option "--cert-name main" when you run
[certbot](https://certbot.eff.org/) to get your cert.

Getting A Cert From Let's Encrypt
---------------------------------

Suppose you have a domain name "mynewproject.com" and you have set up
DNS so that this name points to your server.  Further suppose that you
have launched althttpd to listen on both ports 80 and 443 as shown
in the previous example.  Then to get a cert for that website, you
would run a command like this:

> ~~~
certbot --cert-name main --webroot \
  -w ~/www/mynewproject_com.website -d mynewproject.com
~~~

The previous assumes that the files for your website are in 
"~/www/mynewproject_com.website".  If you instead put them in
"~/www/default.website" then set the argument of the -w option
to "~/www/default.website".

If you have multiple domain names that you want to serve, simply
add additional -w and -d options for each domain.  The -w for the
domain should come first, followed immediately by the -d option.
For example:

> ~~~
certbox --cert-name main --webroot \
  -w ~/www/mynewproject_com.website -d mynewproject.com \
  -w ~/www/secondproject_com.website -d secondproject.com \
  -w ~/www/thirdproject_com.website -d thirdproject.com
~~~

When certbox runs successfully, it will create a new cert file for
you in the /etc/letsencrypt/live/main directory.  Then you simply
stop and restart althttpd, using exactly the same command as shown
previously, but now you can visit your site using "https://" URLs.

Automatic Renewal Of Certs
--------------------------

To automatically renew certs, set up a cron job to run a command like
the following weekly, or even daily:

> ~~~
certbot renew
~~~

You will need to stop and restart the althttpd webserver whenever the
cert updates.  You can cause this to open automatically by putting a
script in your /etc/letsencrypt/renewal-hooks/post directory that kills
off the old copy of althttpd and starts up a new one.


Stand-alone mode for website development and testing
----------------------------------------------------

If you have in a directory the various HTML, Javascript, CSS, and other
resource files that together comprise a website and you want to test those
files easily, you can type a command like:

> ~~~
althttpd --page index.html
~~~

In the above, "`index.html`" is the name of the initial HTML page.  This
command starts althttpd in stand-alone mode, listening on the first
available port it can find, and bound to the loopback IP address
(127.0.0.1).  It also automatically brings up a new tab in your web-browser
and points it to the "index.html" page.

If you are developing the website on a remote system, you can start up
as follows:

> ~~~
althttpd --popup
~~~

The "--popup" option works similarly to "--page" except that it does
not restrict the IP address to loopback and it does not attempt to start
up a new web browser tab.
