﻿/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
#ifdef MYSQL_CLIENT
  {"ssl-mode", OPT_SSL_MODE,
   "SSL connection mode.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ssl", OPT_SSL_SSL,
   "Deprecated. Use --ssl-mode instead.",
   &opt_use_ssl_arg, &opt_use_ssl_arg, 0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"ssl-verify-server-cert", OPT_SSL_VERIFY_SERVER_CERT,
   "Deprecated. Use --ssl-mode=VERIFY_IDENTITY instead.",
   &opt_ssl_verify_server_cert_arg, &opt_ssl_verify_server_cert_arg,
    0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"ssl", OPT_SSL_SSL,
   "If set to ON, this option enforces that SSL is established before client "
   "attempts to authenticate to the server. To disable client SSL capabilities "
   "use --ssl=OFF.",
   &opt_use_ssl, &opt_use_ssl, 0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
#endif
  {"ssl-ca", OPT_SSL_CA,
   "CA file in PEM format.",
   &opt_ssl_ca, &opt_ssl_ca, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"ssl-capath", OPT_SSL_CAPATH,
   "CA directory.",
   &opt_ssl_capath, &opt_ssl_capath, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"ssl-cert", OPT_SSL_CERT, "X509 cert in PEM format.",
   &opt_ssl_cert, &opt_ssl_cert, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"ssl-cipher", OPT_SSL_CIPHER, "SSL cipher to use.",
   &opt_ssl_cipher, &opt_ssl_cipher, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"ssl-key", OPT_SSL_KEY, "X509 key in PEM format.",
   &opt_ssl_key, &opt_ssl_key, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"ssl-crl", OPT_SSL_CRL, "Certificate revocation list.",
   &opt_ssl_crl, &opt_ssl_crl, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"ssl-crlpath", OPT_SSL_CRLPATH,
    "Certificate revocation list path.",
   &opt_ssl_crlpath, &opt_ssl_crlpath, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
   {"tls-version", OPT_TLS_VERSION, "TLS version to use, "
#ifndef HAVE_YASSL
    "permitted values are: TLSv1, TLSv1.1, TLSv1.2",
#else
    "permitted values are: TLSv1, TLSv1.1",
#endif
    &opt_tls_version, &opt_tls_version, 0, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0},
#endif /* HAVE_OPENSSL */



