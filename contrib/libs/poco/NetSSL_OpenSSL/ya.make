# Generated by devtools/yamaker.

LIBRARY()

OWNER(
    orivej
    g:cpp-contrib
)

LICENSE(
    BSD-3-Clause AND
    BSL-1.0
)
 
LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/openssl
    contrib/libs/poco/Crypto
    contrib/libs/poco/Foundation
    contrib/libs/poco/JSON
    contrib/libs/poco/Net
    contrib/libs/poco/Util
    contrib/libs/poco/XML
)

ADDINCL(
    GLOBAL contrib/libs/poco/NetSSL_OpenSSL/include
    contrib/libs/poco/Crypto/include
    contrib/libs/poco/Foundation/include
    contrib/libs/poco/Net/include
    contrib/libs/poco/NetSSL_OpenSSL/src
    contrib/libs/poco/Util/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    src/AcceptCertificateHandler.cpp
    src/CertificateHandlerFactory.cpp
    src/CertificateHandlerFactoryMgr.cpp
    src/ConsoleCertificateHandler.cpp
    src/Context.cpp
    src/HTTPSClientSession.cpp
    src/HTTPSSessionInstantiator.cpp
    src/HTTPSStreamFactory.cpp
    src/InvalidCertificateHandler.cpp
    src/KeyConsoleHandler.cpp
    src/KeyFileHandler.cpp
    src/PrivateKeyFactory.cpp
    src/PrivateKeyFactoryMgr.cpp
    src/PrivateKeyPassphraseHandler.cpp
    src/RejectCertificateHandler.cpp
    src/SSLException.cpp
    src/SSLManager.cpp
    src/SecureSMTPClientSession.cpp
    src/SecureServerSocket.cpp
    src/SecureServerSocketImpl.cpp
    src/SecureSocketImpl.cpp
    src/SecureStreamSocket.cpp
    src/SecureStreamSocketImpl.cpp
    src/Session.cpp
    src/Utility.cpp
    src/VerificationErrorArgs.cpp
    src/X509Certificate.cpp
)

END()
