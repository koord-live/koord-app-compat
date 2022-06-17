/******************************************************************************\
 * Copyright (c) 2004-2022
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
\******************************************************************************/

#include <QCoreApplication>
#include <QtDebug>
#include <QDir>
#include <iostream>
#include "global.h"
#ifndef HEADLESS
#    include <QApplication>
#    include <QtWebView>
#    include <QQuickWindow>
#    include <QMessageBox>
#    include "serverdlg.h"
#    ifndef SERVER_ONLY
#        include "clientdlg.h"
#    endif
#endif
#include "settings.h"
#ifndef SERVER_ONLY
#    include "testbench.h"
#endif
#include "util.h"
#ifdef ANDROID
//#    include <QtAndroidExtras/QtAndroid>
#    include <QtCore/private/qandroidextras_p.h>
#endif
#if defined( Q_OS_MACX )
#    include "mac/activity.h"
extern void qt_set_sequence_auto_mnemonic ( bool bEnable );
#   include "mac/activity.h"
#   include <QFileOpenEvent>
#endif
#include <memory>
#include "rpcserver.h"
#include "serverrpc.h"
#ifndef SERVER_ONLY
#    include "clientrpc.h"
#endif

// Implementation **************************************************************

//// is this the right ifdef or should it be ...
//// #    if defined( __APPLE__ ) || defined( __MACOSX )
//// note: below class definition doesn't compile on Linux
//#if defined( Q_OS_MACX )

//// POC:
//// HACKED duplicated selected code from int main()
//// Adding this for MacOS custom URL handler
//// as per https://doc.qt.io/qt-5/qfileopenevent.html#macos-example
//class KJApplication : public QApplication
//{
//public:
//    KJApplication(int &argc=0, char **argv=0)
//}

//KJApplication::KJApplication( int &argc, char **argv )
//    : QApplication(argc, argv)
//{
//    // all main int main stuff ?


//    // add event handler for koord:// url
//    bool event(QEvent *event) override
//    {
//        if (event->type() == QEvent::FileOpen) {
//            QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
//            qDebug() << "Open URL" << openEvent->url();

//            // Set up QApplication with normal GUI, autoconnect options ...
//            QString      strConnOnStartupAddress     = openEvent->url().toString();
//            quint16      iPortNumber                 = DEFAULT_PORT_NUMBER;
//            quint16      iQosNumber                  = DEFAULT_QOS_NUMBER;
//            QString      strMIDISetup                = "";
//            bool         bNoAutoJackConnect          = false;
//            QString      strClientName               = "";
//            bool         bMuteMeInPersonalMix        = false;
//            // more
//            QString      strIniFileName              = "";
//            QList<QString> CommandLineOptions;
//            bool         bUseTranslation             = true;
//            bool         bShowComplRegConnList       = false;
//            bool         bShowAnalyzerConsole       = false;
//            bool         bMuteStream       = false;

//            // set only option we need
//            CommandLineOptions << "--autoconnect";

//            QCoreApplication* pApp = QCoreApplication::instance();
//            if (pApp == nullptr) {
//                // ?? set args here "artificially"?
//                int i = 2;
//                char *strthing[2];
//                pApp = new QApplication (i, strthing);
//            }

//            // On OSX we need to declare an activity to ensure the process doesn't get
//            // throttled by OS level Nap, Sleep, and Thread Priority systems.
//            CActivity activity;
//            activity.BeginActivity();

//            // init resources
//            Q_INIT_RESOURCE ( resources );

//            try {
//                // Client:
//                // actual client object
//                CClient
//                    Client ( iPortNumber, iQosNumber, strConnOnStartupAddress, strMIDISetup, bNoAutoJackConnect, strClientName, bMuteMeInPersonalMix );
//                // load settings from init-file (command line options override)
//                CClientSettings Settings ( &Client, strIniFileName );
//                Settings.Load ( CommandLineOptions );
//                // load translation
//                if ( bUseTranslation )
//                {
//                    CLocale::LoadTranslation ( Settings.strLanguage, pApp );
//                    CInstPictures::UpdateTableOnLanguageChange();
//                }

//                // GUI object
//                CClientDlg ClientDlg ( &Client,
//                                       &Settings,
//                                       strConnOnStartupAddress,
//                                       strMIDISetup,
//                                       bShowComplRegConnList,
//                                       bShowAnalyzerConsole,
//                                       bMuteStream,
//                                       nullptr );

//                // show dialog
//                ClientDlg.show();
//                pApp->exec();
//            }

//            catch ( const CGenErr& generr ) {
//            // show generic error
//                QMessageBox::critical ( nullptr, APP_NAME, generr.GetErrorText(), "Quit", nullptr );
//            }
//        }

//        return QApplication::event(event);
//    }
//};
//#endif

int main ( int argc, char** argv )
{

#if defined( Q_OS_MACX )
    // Mnemonic keys are default disabled in Qt for MacOS. The following function enables them.
    // Qt will not show these with underline characters in the GUI on MacOS. (#1873)
    qt_set_sequence_auto_mnemonic ( true );
#endif

    QString        strArgument;
    double         rDbleArgument;
    QList<QString> CommandLineOptions;
    QList<QString> ServerOnlyOptions;
    QList<QString> ClientOnlyOptions;

    // initialize all flags and string which might be changed by command line
    // arguments
#if ( defined( SERVER_BUNDLE ) && defined( Q_OS_MACX ) ) || defined( SERVER_ONLY )
    // if we are on MacOS and we are building a server bundle or requested build with serveronly, start Jamulus in server mode
    bool bIsClient = false;
    qInfo() << "- Starting in server mode by default (due to compile time option)";
#else
    bool bIsClient = true;
#endif
    bool         bUseGUI                     = true;
    bool         bStartMinimized             = false;
    bool         bShowComplRegConnList       = false;
    bool         bDisconnectAllClientsOnQuit = false;
    bool         bUseDoubleSystemFrameSize   = true; // default is 128 samples frame size
    bool         bUseMultithreading          = false;
    bool         bShowAnalyzerConsole        = false;
    bool         bMuteStream                 = false;
    bool         bMuteMeInPersonalMix        = false;
    bool         bDisableRecording           = false;
    bool         bDelayPan                   = false;
    bool         bNoAutoJackConnect          = false;
    bool         bUseTranslation             = true;
    bool         bCustomPortNumberGiven      = false;
    bool         bEnableIPv6                 = false;
    int          iNumServerChannels          = DEFAULT_USED_NUM_CHANNELS;
    quint16      iPortNumber                 = DEFAULT_PORT_NUMBER;
    int          iJsonRpcPortNumber          = INVALID_PORT;
    quint16      iQosNumber                  = DEFAULT_QOS_NUMBER;
    ELicenceType eLicenceType                = LT_NO_LICENCE;
    QString      strMIDISetup                = "";
    QString      strConnOnStartupAddress     = "";
    QString      strIniFileName              = "";
    QString      strHTMLStatusFileName       = "";
    QString      strLoggingFileName          = "";
    QString      strRecordingDirName         = "";
    QString      strDirectoryServer          = "";
    QString      strServerListFileName       = "";
    QString      strServerInfo               = "";
    QString      strServerPublicIP           = "";
    QString      strServerBindIP             = "";
    QString      strServerListFilter         = "";
    QString      strWelcomeMessage           = "";
    QString      strClientName               = "";
    QString      strJsonRpcSecretFileName    = "";

#if !defined( HEADLESS ) && defined( _WIN32 )
    if ( AttachConsole ( ATTACH_PARENT_PROCESS ) )
    {
        freopen ( "CONOUT$", "w", stdout );
        freopen ( "CONOUT$", "w", stderr );
    }
#endif

    // When adding new options, follow the same order as --help output

    // QT docu: argv()[0] is the program name, argv()[1] is the first
    // argument and argv()[argc()-1] is the last argument.
    // Start with first argument, therefore "i = 1"
    for ( int i = 1; i < argc; i++ )
    {

        // Help (usage) flag ---------------------------------------------------
        if ( ( !strcmp ( argv[i], "--help" ) ) || ( !strcmp ( argv[i], "-h" ) ) || ( !strcmp ( argv[i], "-?" ) ) )
        {
            const QString strHelp = UsageArguments ( argv );
            std::cout << qUtf8Printable ( strHelp );
            exit ( 0 );
        }

        // Version number ------------------------------------------------------
        if ( ( !strcmp ( argv[i], "--version" ) ) || ( !strcmp ( argv[i], "-v" ) ) )
        {
            std::cout << qUtf8Printable ( GetVersionAndNameStr ( false ) );
            exit ( 0 );
        }

        // Common options:

        // Initialization file -------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-i", "--inifile", strArgument ) )
        {
            strIniFileName = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- initialization file name: %1" ).arg ( strIniFileName ) );
            CommandLineOptions << "--inifile";
            continue;
        }

        // Disable GUI flag ----------------------------------------------------
        if ( GetFlagArgument ( argv, i, "-n", "--nogui" ) )
        {
            bUseGUI = false;
            qInfo() << "- no GUI mode chosen";
            CommandLineOptions << "--nogui";
            continue;
        }

        // Port number ---------------------------------------------------------
        if ( GetNumericArgument ( argc, argv, i, "-p", "--port", 0, 65535, rDbleArgument ) )
        {
            iPortNumber            = static_cast<quint16> ( rDbleArgument );
            bCustomPortNumberGiven = true;
            qInfo() << qUtf8Printable ( QString ( "- selected port number: %1" ).arg ( iPortNumber ) );
            CommandLineOptions << "--port";
            continue;
        }

        // JSON-RPC port number ------------------------------------------------
        if ( GetNumericArgument ( argc, argv, i, "--jsonrpcport", "--jsonrpcport", 0, 65535, rDbleArgument ) )
        {
            iJsonRpcPortNumber = static_cast<quint16> ( rDbleArgument );
            qInfo() << qUtf8Printable ( QString ( "- JSON-RPC port number: %1" ).arg ( iJsonRpcPortNumber ) );
            CommandLineOptions << "--jsonrpcport";
            continue;
        }

        // JSON-RPC secret file name -------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "--jsonrpcsecretfile", "--jsonrpcsecretfile", strArgument ) )
        {
            strJsonRpcSecretFileName = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- JSON-RPC secret file: %1" ).arg ( strJsonRpcSecretFileName ) );
            CommandLineOptions << "--jsonrpcsecretfile";
            continue;
        }

        // Quality of Service --------------------------------------------------
        if ( GetNumericArgument ( argc, argv, i, "-Q", "--qos", 0, 255, rDbleArgument ) )
        {
            iQosNumber = static_cast<quint16> ( rDbleArgument );
            qInfo() << qUtf8Printable ( QString ( "- selected QoS value: %1" ).arg ( iQosNumber ) );
            CommandLineOptions << "--qos";
            continue;
        }

        // Disable translations ------------------------------------------------
        if ( GetFlagArgument ( argv, i, "-t", "--notranslation" ) )
        {
            bUseTranslation = false;
            qInfo() << "- translations disabled";
            CommandLineOptions << "--notranslation";
            continue;
        }

        // Enable IPv6 ---------------------------------------------------------
        if ( GetFlagArgument ( argv, i, "-6", "--enableipv6" ) )
        {
            bEnableIPv6 = true;
            qInfo() << "- IPv6 enabled";
            CommandLineOptions << "--enableipv6";
            continue;
        }

        // Server only:

        // Disconnect all clients on quit --------------------------------------
        if ( GetFlagArgument ( argv, i, "-d", "--discononquit" ) )
        {
            bDisconnectAllClientsOnQuit = true;
            qInfo() << "- disconnect all clients on quit";
            CommandLineOptions << "--discononquit";
            ServerOnlyOptions << "--discononquit";
            continue;
        }

        // Directory server ----------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-e", "--directoryserver", strArgument ) )
        {
            strDirectoryServer = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- directory server: %1" ).arg ( strDirectoryServer ) );
            CommandLineOptions << "--directoryserver";
            ServerOnlyOptions << "--directoryserver";
            continue;
        }

        // Central server ** D E P R E C A T E D ** ----------------------------
        if ( GetStringArgument ( argc,
                                 argv,
                                 i,
                                 "--centralserver", // no short form
                                 "--centralserver",
                                 strArgument ) )
        {
            strDirectoryServer = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- directory server: %1" ).arg ( strDirectoryServer ) );
            CommandLineOptions << "--directoryserver";
            ServerOnlyOptions << "--directoryserver";
            continue;
        }

        // Directory file ------------------------------------------------------
        if ( GetStringArgument ( argc,
                                 argv,
                                 i,
                                 "--directoryfile", // no short form
                                 "--directoryfile",
                                 strArgument ) )
        {
            strServerListFileName = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- directory server persistence file: %1" ).arg ( strServerListFileName ) );
            CommandLineOptions << "--directoryfile";
            ServerOnlyOptions << "--directoryfile";
            continue;
        }

        // Server list filter --------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-f", "--listfilter", strArgument ) )
        {
            strServerListFilter = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- server list filter: %1" ).arg ( strServerListFilter ) );
            CommandLineOptions << "--listfilter";
            ServerOnlyOptions << "--listfilter";
            continue;
        }

        // Use 64 samples frame size mode --------------------------------------
        if ( GetFlagArgument ( argv, i, "-F", "--fastupdate" ) )
        {
            bUseDoubleSystemFrameSize = false; // 64 samples frame size
            qInfo() << qUtf8Printable ( QString ( "- using %1 samples frame size mode" ).arg ( SYSTEM_FRAME_SIZE_SAMPLES ) );
            CommandLineOptions << "--fastupdate";
            ServerOnlyOptions << "--fastupdate";
            continue;
        }

        // Use logging ---------------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-l", "--log", strArgument ) )
        {
            strLoggingFileName = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- logging file name: %1" ).arg ( strLoggingFileName ) );
            CommandLineOptions << "--log";
            ServerOnlyOptions << "--log";
            continue;
        }

        // Use licence flag ----------------------------------------------------
        if ( GetFlagArgument ( argv, i, "-L", "--licence" ) )
        {
            // LT_CREATIVECOMMONS is now used just to enable the pop up
            eLicenceType = LT_CREATIVECOMMONS;
            qInfo() << "- licence required";
            CommandLineOptions << "--licence";
            ServerOnlyOptions << "--licence";
            continue;
        }

        // HTML status file ----------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-m", "--htmlstatus", strArgument ) )
        {
            strHTMLStatusFileName = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- HTML status file name: %1" ).arg ( strHTMLStatusFileName ) );
            CommandLineOptions << "--htmlstatus";
            ServerOnlyOptions << "--htmlstatus";
            continue;
        }

        // Server info ---------------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-o", "--serverinfo", strArgument ) )
        {
            strServerInfo = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- server info: %1" ).arg ( strServerInfo ) );
            CommandLineOptions << "--serverinfo";
            ServerOnlyOptions << "--serverinfo";
            continue;
        }

        // Server Public IP ----------------------------------------------------
        if ( GetStringArgument ( argc,
                                 argv,
                                 i,
                                 "--serverpublicip", // no short form
                                 "--serverpublicip",
                                 strArgument ) )
        {
            strServerPublicIP = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- server public IP: %1" ).arg ( strServerPublicIP ) );
            CommandLineOptions << "--serverpublicip";
            ServerOnlyOptions << "--serverpublicip";
            continue;
        }

        // Enable delay panning on startup -------------------------------------
        if ( GetFlagArgument ( argv, i, "-P", "--delaypan" ) )
        {
            bDelayPan = true;
            qInfo() << "- starting with delay panning";
            CommandLineOptions << "--delaypan";
            ServerOnlyOptions << "--delaypan";
            continue;
        }

        // Recording directory -------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-R", "--recording", strArgument ) )
        {
            strRecordingDirName = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- recording directory name: %1" ).arg ( strRecordingDirName ) );
            CommandLineOptions << "--recording";
            ServerOnlyOptions << "--recording";
            continue;
        }

        // Disable recording on startup ----------------------------------------
        if ( GetFlagArgument ( argv,
                               i,
                               "--norecord", // no short form
                               "--norecord" ) )
        {
            bDisableRecording = true;
            qInfo() << "- recording will not take place until enabled";
            CommandLineOptions << "--norecord";
            ServerOnlyOptions << "--norecord";
            continue;
        }

        // Server mode flag ----------------------------------------------------
        if ( GetFlagArgument ( argv, i, "-s", "--server" ) )
        {
            bIsClient = false;
            qInfo() << "- server mode chosen";
            CommandLineOptions << "--server";
            ServerOnlyOptions << "--server";
            continue;
        }

        // Server Bind IP --------------------------------------------------
        if ( GetStringArgument ( argc,
                                 argv,
                                 i,
                                 "--serverbindip", // no short form
                                 "--serverbindip",
                                 strArgument ) )
        {
            strServerBindIP = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- server bind IP: %1" ).arg ( strServerBindIP ) );
            CommandLineOptions << "--serverbindip";
            ServerOnlyOptions << "--serverbindip";
            continue;
        }

        // Use multithreading --------------------------------------------------
        if ( GetFlagArgument ( argv, i, "-T", "--multithreading" ) )
        {
            bUseMultithreading = true;
            qInfo() << "- using multithreading";
            CommandLineOptions << "--multithreading";
            ServerOnlyOptions << "--multithreading";
            continue;
        }

        // Maximum number of channels ------------------------------------------
        if ( GetNumericArgument ( argc, argv, i, "-u", "--numchannels", 1, MAX_NUM_CHANNELS, rDbleArgument ) )
        {
            iNumServerChannels = static_cast<int> ( rDbleArgument );

            qInfo() << qUtf8Printable ( QString ( "- maximum number of channels: %1" ).arg ( iNumServerChannels ) );

            CommandLineOptions << "--numchannels";
            ServerOnlyOptions << "--numchannels";
            continue;
        }

        // Server welcome message ----------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-w", "--welcomemessage", strArgument ) )
        {
            strWelcomeMessage = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- welcome message: %1" ).arg ( strWelcomeMessage ) );
            CommandLineOptions << "--welcomemessage";
            ServerOnlyOptions << "--welcomemessage";
            continue;
        }

        // Start minimized -----------------------------------------------------
        if ( GetFlagArgument ( argv, i, "-z", "--startminimized" ) )
        {
            bStartMinimized = true;
            qInfo() << "- start minimized enabled";
            CommandLineOptions << "--startminimized";
            ServerOnlyOptions << "--startminimized";
            continue;
        }

        // Client only:

        // Connect on startup --------------------------------------------------
        if ( GetStringArgument ( argc, argv, i, "-c", "--connect", strArgument ) )
        {
            strConnOnStartupAddress = NetworkUtil::FixAddress ( strArgument );
            qInfo() << qUtf8Printable ( QString ( "- connect on startup to address: %1" ).arg ( strConnOnStartupAddress ) );
            CommandLineOptions << "--connect";
            ClientOnlyOptions << "--connect";
            continue;
        }

        // Disabling auto Jack connections -------------------------------------
        if ( GetFlagArgument ( argv, i, "-j", "--nojackconnect" ) )
        {
            bNoAutoJackConnect = true;
            qInfo() << "- disable auto Jack connections";
            CommandLineOptions << "--nojackconnect";
            ClientOnlyOptions << "--nojackconnect";
            continue;
        }

        // Autoconnect on startup - koord URI  eg koord://XX.XX.XX.XX ------------
        if ( GetStringArgument ( argc, argv, i, "-x", "--autoconnect", strArgument ) )
        {
            strConnOnStartupAddress = NetworkUtil::FixJamAddress ( strArgument );
            qInfo() << qUtf8Printable ( QString ( "- autoconnect on startup to address: %1" ).arg ( strConnOnStartupAddress ) );
            CommandLineOptions << "--autoconnect";
            continue;
        }

        // If single argument ie argc=2 check to see if direct exec of /usr/share/applications/koordrt.desktopkoord url --------------------------------------
        if ( argc == 2) {
//            // if argv[1] matches "koord://{IPv4_addr}"
//            QRegExp rx_gen1("^koord\\:\\/\\/(([0-9]{1,3}\\.){3}[0-9]{1,3})");
//            // gen2 url - if argv[1] matches "koord://{IPv4_addr}:{port}"
//            QRegExp rx_gen2("^koord\\:\\/\\/(([0-9]{1,3}\\.){3}[0-9]{1,3}:[0-9]{3,5})");
//            int pos_gen1 = rx_gen1.indexIn(argv[1]); // match gen1 url
//            int pos_gen2 = rx_gen2.indexIn(argv[1]); // match gen2 url
//            if (pos_gen2 != -1) { // try to match gen2 url first
//                // add -x {IPv4_addr} to CommandLineOptions
//                strConnOnStartupAddress = rx_gen2.cap(1);
//                qInfo() << qUtf8Printable ( QString ( "- autoconnect on startup to address: %1" ).arg ( strConnOnStartupAddress ) );
//                CommandLineOptions << "--autoconnect";
//                continue;
//            } else if (pos_gen1 != -1) { // if no joy, try to match gen1 url
//                // add -x {IPv4_addr} to CommandLineOptions
//                strConnOnStartupAddress = rx_gen1.cap(1);
//                qInfo() << qUtf8Printable ( QString ( "- autoconnect on startup to address: %1" ).arg ( strConnOnStartupAddress ) );
//                CommandLineOptions << "--autoconnect";
//                continue;
//            }

            // if argv[1] matches "koord://{IPv4_addr}"
            QRegularExpression rx_gen1("^koord\\:\\/\\/(([0-9]{1,3}\\.){3}[0-9]{1,3})");
            QRegularExpressionMatch gen1_match = rx_gen1.match(argv[1]);
            // gen2 url - if argv[1] matches "koord://{IPv4_addr}:{port}"
            QRegularExpression rx_gen2("^koord\\:\\/\\/(([0-9]{1,3}\\.){3}[0-9]{1,3}:[0-9]{3,5})");
            QRegularExpressionMatch gen2_match = rx_gen2.match(argv[1]);

            if (gen2_match.hasMatch()) {
                // add -x {IPv4_addr} to CommandLineOptions
                strConnOnStartupAddress = gen2_match.captured(1);
                qInfo() << qUtf8Printable ( QString ( "- autoconnect on startup to address: %1" ).arg ( strConnOnStartupAddress ) );
                CommandLineOptions << "--autoconnect";
                continue;
            } else if (gen1_match.hasMatch()) { // if no joy, try to match gen1 url
                // add -x {IPv4_addr} to CommandLineOptions
                strConnOnStartupAddress = gen1_match.captured(1);
                qInfo() << qUtf8Printable ( QString ( "- autoconnect on startup to address: %1" ).arg ( strConnOnStartupAddress ) );
                CommandLineOptions << "--autoconnect";
                continue;
            }
        }

        // Mute stream on startup ----------------------------------------------
        if ( GetFlagArgument ( argv, i, "-M", "--mutestream" ) )
        {
            bMuteStream = true;
            qInfo() << "- mute stream activated";
            CommandLineOptions << "--mutestream";
            ClientOnlyOptions << "--mutestream";
            continue;
        }

        // For headless client mute my own signal in personal mix --------------
        if ( GetFlagArgument ( argv,
                               i,
                               "--mutemyown", // no short form
                               "--mutemyown" ) )
        {
            bMuteMeInPersonalMix = true;
            qInfo() << "- mute me in my personal mix";
            CommandLineOptions << "--mutemyown";
            ClientOnlyOptions << "--mutemyown";
            continue;
        }

        // Client Name ---------------------------------------------------------
        if ( GetStringArgument ( argc,
                                 argv,
                                 i,
                                 "--clientname", // no short form
                                 "--clientname",
                                 strArgument ) )
        {
            strClientName = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- client name: %1" ).arg ( strClientName ) );
            CommandLineOptions << "--clientname";
            ClientOnlyOptions << "--clientname";
            continue;
        }

        // Controller MIDI channel ---------------------------------------------
        if ( GetStringArgument ( argc,
                                 argv,
                                 i,
                                 "--ctrlmidich", // no short form
                                 "--ctrlmidich",
                                 strArgument ) )
        {
            strMIDISetup = strArgument;
            qInfo() << qUtf8Printable ( QString ( "- MIDI controller settings: %1" ).arg ( strMIDISetup ) );
            CommandLineOptions << "--ctrlmidich";
            ClientOnlyOptions << "--ctrlmidich";
            continue;
        }

        // Undocumented:

        // Show all registered servers in the server list ----------------------
        // Undocumented debugging command line argument: Show all registered
        // servers in the server list regardless if a ping to the server is
        // possible or not.
        if ( GetFlagArgument ( argv,
                               i,
                               "--showallservers", // no short form
                               "--showallservers" ) )
        {
            bShowComplRegConnList = true;
            qInfo() << "- show all registered servers in server list";
            CommandLineOptions << "--showallservers";
            ClientOnlyOptions << "--showallservers";
            continue;
        }

        // Show analyzer console -----------------------------------------------
        // Undocumented debugging command line argument: Show the analyzer
        // console to debug network buffer properties.
        if ( GetFlagArgument ( argv,
                               i,
                               "--showanalyzerconsole", // no short form
                               "--showanalyzerconsole" ) )
        {
            bShowAnalyzerConsole = true;
            qInfo() << "- show analyzer console";
            CommandLineOptions << "--showanalyzerconsole";
            ClientOnlyOptions << "--showanalyzerconsole";
            continue;
        }

        // Unknown option ------------------------------------------------------
        qCritical() << qUtf8Printable ( QString ( "%1: Unknown option '%2' -- use '--help' for help" ).arg ( argv[0] ).arg ( argv[i] ) );

// clicking on the Mac application bundle, the actual application
// is called with weird command line args -> do not exit on these
#if !( defined( Q_OS_MACX ) )
        exit ( 1 );
#endif
    }

    // Dependencies ------------------------------------------------------------
#ifdef HEADLESS
    if ( bUseGUI )
    {
        bUseGUI = false;
        qWarning() << "No GUI support compiled. Running in headless mode.";
    }
    Q_UNUSED ( bStartMinimized )       // avoid compiler warnings
    Q_UNUSED ( bShowComplRegConnList ) // avoid compiler warnings
    Q_UNUSED ( bShowAnalyzerConsole )  // avoid compiler warnings
    Q_UNUSED ( bMuteStream )           // avoid compiler warnings
#endif

#ifdef SERVER_ONLY
    if ( bIsClient )
    {
        qCritical() << "Only --server mode is supported in this build.";
        exit ( 1 );
    }
#endif

    // TODO create settings in default state, if loading from file do that next, then come back here to
    //      override from command line options, then create client or server, letting them do the validation

    if ( bIsClient )
    {
        if ( ServerOnlyOptions.size() != 0 )
        {
            qCritical() << qUtf8Printable ( QString ( "%1: Server only option(s) '%2' used.  Did you omit '--server'?" )
                                                .arg ( argv[0] )
                                                .arg ( ServerOnlyOptions.join ( ", " ) ) );
            exit ( 1 );
        }

        // mute my own signal in personal mix is only supported for headless mode
        if ( bUseGUI && bMuteMeInPersonalMix )
        {
            bMuteMeInPersonalMix = false;
            qWarning() << "Mute my own signal in my personal mix is only supported in headless mode.";
        }

        // adjust default port number for client: use different default port than the server since
        // if the client is started before the server, the server would get a socket bind error
        if ( !bCustomPortNumberGiven )
        {
            iPortNumber += 10; // increment by 10
            qInfo() << qUtf8Printable ( QString ( "- allocated port number: %1" ).arg ( iPortNumber ) );
        }
    }
    else
    {
        if ( ClientOnlyOptions.size() != 0 )
        {
            qCritical() << qUtf8Printable (
                QString ( "%1: Client only option(s) '%2' used.  See '--help' for help" ).arg ( argv[0] ).arg ( ClientOnlyOptions.join ( ", " ) ) );
            exit ( 1 );
        }

        if ( bUseGUI )
        {
            // by definition, when running with the GUI we always default to registering somewhere but
            // until the settings are loaded we do not know where, so we cannot be prescriptive here

            if ( !strServerListFileName.isEmpty() )
            {
                qInfo() << "Note:"
                        << "Server list persistence file will only take effect when running as a directory server.";
            }

            if ( !strServerListFilter.isEmpty() )
            {
                qInfo() << "Note:"
                        << "Server list filter will only take effect when running as a directory server.";
            }
        }
        else
        {
            // the inifile is not supported for the headless server mode
            if ( !strIniFileName.isEmpty() )
            {
                qWarning() << "No initialization file support in headless server mode.";
                strIniFileName = "";
            }
            // therefore we know everything based on command line options

            if ( strDirectoryServer.compare ( "localhost", Qt::CaseInsensitive ) == 0 || strDirectoryServer.compare ( "127.0.0.1" ) == 0 )
            {
                if ( !strServerListFileName.isEmpty() )
                {
                    QFileInfo serverListFileInfo ( strServerListFileName );
                    if ( !serverListFileInfo.exists() )
                    {
                        QFile strServerListFile ( strServerListFileName );
                        if ( !strServerListFile.open ( QFile::OpenModeFlag::ReadWrite ) )
                        {
                            qWarning() << qUtf8Printable (
                                QString ( "Cannot create %1 for reading and writing.  Please check permissions." ).arg ( strServerListFileName ) );
                            strServerListFileName = "";
                        }
                    }
                    else if ( !serverListFileInfo.isFile() )
                    {
                        qWarning() << qUtf8Printable (
                            QString ( "Server list file %1 must be a plain file.  Please check the name." ).arg ( strServerListFileName ) );
                        strServerListFileName = "";
                    }
                    else if ( !serverListFileInfo.isReadable() || !serverListFileInfo.isWritable() )
                    {
                        qWarning() << qUtf8Printable (
                            QString ( "Server list file %1 must be readable and writeable.  Please check the permissions." )
                                .arg ( strServerListFileName ) );
                        strServerListFileName = "";
                    }
                }

                if ( !strServerListFilter.isEmpty() )
                {
                    QStringList slWhitelistAddresses = strServerListFilter.split ( ";" );
                    for ( int iIdx = 0; iIdx < slWhitelistAddresses.size(); iIdx++ )
                    {
                        // check for special case: [version]
                        if ( ( slWhitelistAddresses.at ( iIdx ).length() > 2 ) && ( slWhitelistAddresses.at ( iIdx ).left ( 1 ) == "[" ) &&
                             ( slWhitelistAddresses.at ( iIdx ).right ( 1 ) == "]" ) )
                        {
                            // Good case - it seems QVersionNumber isn't fussy
                        }
                        else if ( slWhitelistAddresses.at ( iIdx ).isEmpty() )
                        {
                            qWarning() << "There is empty entry in the server list filter that will be ignored";
                        }
                        else
                        {
                            QHostAddress InetAddr;
                            if ( !InetAddr.setAddress ( slWhitelistAddresses.at ( iIdx ) ) )
                            {
                                qWarning() << qUtf8Printable (
                                    QString ( "%1 is not a valid server list filter entry. Only plain IP addresses are supported" )
                                        .arg ( slWhitelistAddresses.at ( iIdx ) ) );
                            }
                        }
                    }
                }
            }
            else
            {
                if ( !strServerListFileName.isEmpty() )
                {
                    qWarning() << "Server list persistence file will only take effect when running as a directory server.";
                    strServerListFileName = "";
                }

                if ( !strServerListFilter.isEmpty() )
                {
                    qWarning() << "Server list filter will only take effect when running as a directory server.";
                    strServerListFileName = "";
                }
            }

            if ( strDirectoryServer.isEmpty() )
            {
                if ( !strServerPublicIP.isEmpty() )
                {
                    qWarning() << "Server Public IP will only take effect when registering a server with a directory server.";
                    strServerPublicIP = "";
                }
            }
            else
            {
                if ( !strServerPublicIP.isEmpty() )
                {
                    QHostAddress InetAddr;
                    if ( !InetAddr.setAddress ( strServerPublicIP ) )
                    {
                        qWarning() << "Server Public IP is invalid. Only plain IP addresses are supported.";
                        strServerPublicIP = "";
                    }
                }
            }
        }

        if ( !strServerBindIP.isEmpty() )
        {
            QHostAddress InetAddr;
            if ( !InetAddr.setAddress ( strServerBindIP ) )
            {
                qWarning() << "Server Bind IP is invalid. Only plain IP addresses are supported.";
                strServerBindIP = "";
            }
        }
    }

    // Application/GUI setup ---------------------------------------------------
    // Application object
#ifdef HEADLESS
    QCoreApplication* pApp = new QCoreApplication ( argc, argv );
#else
#    if defined( Q_OS_IOS )
    bUseGUI        = true;
    bIsClient      = true; // Client only - TODO: maybe a switch in interface to change to server?

    // bUseMultithreading = true;
    QApplication* pApp = new QApplication ( argc, argv );
#    else

    // need to set OpenGL specifically for at least Mac!
    // https://doc.qt.io/qt-6/qquickwidget.html#performance-considerations
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    // https://doc.qt.io/qt-6/qml-qtwebengine-webengineview.html#details
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
//    QtWebEngineQuick::initialize();
    // need this before new QApplication
    QtWebView::initialize();

    //FIXME - gui vs nogui handling
//    QCoreApplication* pApp = bUseGUI ? new QApplication ( argc, argv ) : new QCoreApplication ( argc, argv );
    QApplication* pApp = bUseGUI ? new QApplication ( argc, argv ) : new QApplication ( argc, argv );
    pApp->setStyle("fusion");

    // Now use a palette to switch to dark colors:
    QPalette palette = QPalette();
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(25, 25, 25));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, Qt::black);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    pApp->setPalette(palette);

#    endif
#endif

#ifdef ANDROID
    // special Android code needed for record audio permission handling
    auto permcheck = QtAndroidPrivate::checkPermission( QString ("android.permission.RECORD_AUDIO"));

    if ( permcheck.result() == QtAndroidPrivate::PermissionResult::Denied)
    {
        auto reqPermRes = QtAndroidPrivate::requestPermission( "android.permission.RECORD_AUDIO" );

        if ( reqPermRes.result() == QtAndroidPrivate::PermissionResult::Denied)
        {
            return 0;
        }
    }
#endif

#ifdef _WIN32
    // set application priority class -> high priority
    SetPriorityClass ( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

    // For accessible support we need to add a plugin to qt. The plugin has to
    // be located in the install directory of the software by the installer.
    // Here, we set the path to our application path.
    QDir ApplDir ( QApplication::applicationDirPath() );
    pApp->addLibraryPath ( QString ( ApplDir.absolutePath() ) );
#endif

#if defined( Q_OS_MACX )
    // On OSX we need to declare an activity to ensure the process doesn't get
    // throttled by OS level Nap, Sleep, and Thread Priority systems.
    CActivity activity;

    activity.BeginActivity();
#endif

    // init resources
    Q_INIT_RESOURCE ( resources );

#ifndef SERVER_ONLY
    // clang-format off
// TEST -> activate the following line to activate the test bench,
//CTestbench Testbench ( "127.0.0.1", DEFAULT_PORT_NUMBER );
// clang-format on
#endif

    CRpcServer* pRpcServer = nullptr;

    if ( iJsonRpcPortNumber != INVALID_PORT )
    {
        if ( strJsonRpcSecretFileName.isEmpty() )
        {
            qCritical() << qUtf8Printable ( QString ( "- JSON-RPC: --jsonrpcsecretfile is required. Exiting." ) );
            exit ( 1 );
        }

        QFile qfJsonRpcSecretFile ( strJsonRpcSecretFileName );
        if ( !qfJsonRpcSecretFile.open ( QFile::OpenModeFlag::ReadOnly ) )
        {
            qCritical() << qUtf8Printable ( QString ( "- JSON-RPC: Unable to open secret file %1. Exiting." ).arg ( strJsonRpcSecretFileName ) );
            exit ( 1 );
        }
        QTextStream qtsJsonRpcSecretStream ( &qfJsonRpcSecretFile );
        QString     strJsonRpcSecret = qtsJsonRpcSecretStream.readLine();
        if ( strJsonRpcSecret.length() < JSON_RPC_MINIMUM_SECRET_LENGTH )
        {
            qCritical() << qUtf8Printable ( QString ( "JSON-RPC: Refusing to run with secret of length %1 (required: %2). Exiting." )
                                                .arg ( strJsonRpcSecret.length() )
                                                .arg ( JSON_RPC_MINIMUM_SECRET_LENGTH ) );
            exit ( 1 );
        }

        qWarning() << "- JSON-RPC: This interface is experimental and is subject to breaking changes even on patch versions "
                      "(not subject to semantic versioning) during the initial phase.";

        pRpcServer = new CRpcServer ( pApp, iJsonRpcPortNumber, strJsonRpcSecret );
        if ( !pRpcServer->Start() )
        {
            qCritical() << qUtf8Printable ( QString ( "- JSON-RPC: Server failed to start. Exiting." ) );
            exit ( 1 );
        }
    }

    try
    {
#ifndef SERVER_ONLY
        if ( bIsClient )
        {
            // Client:
            // actual client object
            CClient Client ( iPortNumber,
                             iQosNumber,
                             strConnOnStartupAddress,
                             strMIDISetup,
                             bNoAutoJackConnect,
                             strClientName,
                             bEnableIPv6,
                             bMuteMeInPersonalMix );

            // load settings from init-file (command line options override)
            CClientSettings Settings ( &Client, strIniFileName );
            Settings.Load ( CommandLineOptions );

            // load translation
            if ( bUseGUI && bUseTranslation )
            {
                CLocale::LoadTranslation ( Settings.strLanguage, pApp );
                CInstPictures::UpdateTableOnLanguageChange();
            }

            if ( pRpcServer )
            {
                new CClientRpc ( &Client, pRpcServer, pRpcServer );
            }

#    ifndef HEADLESS
            if ( bUseGUI )
            {
                // GUI object
                CClientDlg ClientDlg ( &Client,
                                       &Settings,
                                       strConnOnStartupAddress,
                                       strMIDISetup,
                                       bShowComplRegConnList,
                                       bShowAnalyzerConsole,
                                       bMuteStream,
                                       bEnableIPv6,
                                       nullptr );

                // show dialog
                ClientDlg.show();
                pApp->exec();
            }
            else
#    endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                pApp->exec();
            }
        }
        else
#endif
        {
            // Server:
            // actual server object
            CServer Server ( iNumServerChannels,
                             strLoggingFileName,
                             strServerBindIP,
                             iPortNumber,
                             iQosNumber,
                             strHTMLStatusFileName,
                             strDirectoryServer,
                             strServerListFileName,
                             strServerInfo,
                             strServerPublicIP,
                             strServerListFilter,
                             strWelcomeMessage,
                             strRecordingDirName,
                             bDisconnectAllClientsOnQuit,
                             bUseDoubleSystemFrameSize,
                             bUseMultithreading,
                             bDisableRecording,
                             bDelayPan,
                             bEnableIPv6,
                             eLicenceType );

            if ( pRpcServer )
            {
                new CServerRpc ( &Server, pRpcServer, pRpcServer );
            }

#ifndef HEADLESS
            if ( bUseGUI )
            {
                // load settings from init-file (command line options override)
                CServerSettings Settings ( &Server, strIniFileName );
                Settings.Load ( CommandLineOptions );

                // load translation
                if ( bUseGUI && bUseTranslation )
                {
                    CLocale::LoadTranslation ( Settings.strLanguage, pApp );
                }

                // GUI object for the server
                CServerDlg ServerDlg ( &Server, &Settings, bStartMinimized, nullptr );

                // show dialog (if not the minimized flag is set)
                if ( !bStartMinimized )
                {
                    ServerDlg.show();
                }

                pApp->exec();
            }
            else
#endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                // CServerListManager defaults to AT_NONE, so need to switch if
                // strDirectoryServer is wanted
                if ( !strDirectoryServer.isEmpty() )
                {
                    Server.SetDirectoryType ( AT_CUSTOM );
                }

                pApp->exec();
            }
        }
    }

    catch ( const CGenErr& generr )
    {
        // show generic error
#ifndef HEADLESS
        if ( bUseGUI )
        {
            QMessageBox::critical ( nullptr, APP_NAME, generr.GetErrorText(), "Quit", nullptr );
        }
        else
#endif
        {
            qCritical() << qUtf8Printable ( QString ( "%1: %2" ).arg ( APP_NAME ).arg ( generr.GetErrorText() ) );
            exit ( 1 );
        }
    }

#if defined( Q_OS_MACX )
    activity.EndActivity();
#endif

    return 0;
}


/******************************************************************************\
* Command Line Argument Parsing                                                *
\******************************************************************************/
QString UsageArguments ( char** argv )
{
    // clang-format off
    return QString (
           "\n"
           "Usage: %1 [option] [option argument] ...\n"
           "\n"
           "  -h, -?, --help        display this help text and exit\n"
           "  -v, --version         display version information and exit\n"
           "\n"
           "Common options:\n"
           "  -i, --inifile         initialization file name\n"
           "                        (not supported for headless Server mode)\n"
           "  -n, --nogui           disable GUI (\"headless\")\n"
           "  -p, --port            set the local port number\n"
           "      --jsonrpcport     enable JSON-RPC server, set TCP port number\n"
           "                        (EXPERIMENTAL, APIs might still change;\n"
           "                        only accessible from localhost)\n"
           "      --jsonrpcsecretfile\n"
           "                        path to a single-line file which contains a freely\n"
           "                        chosen secret to authenticate JSON-RPC users.\n"
           "  -Q, --qos             set the QoS value. Default is 128. Disable with 0\n"
           "                        (see the Jamulus website to enable QoS on Windows)\n"
           "  -t, --notranslation   disable translation (use English language)\n"
           "  -6, --enableipv6      enable IPv6 addressing (IPv4 is always enabled)\n"
           "\n"
           "Server only:\n"
           "  -d, --discononquit    disconnect all Clients on quit\n"
           "  -e, --directoryserver address of the directory Server with which to register\n"
           "                        (or 'localhost' to host a server list on this Server)\n"
           "      --directoryfile   Remember registered Servers even if the Directory is restarted. Directory Servers only.\n"
           "  -f, --listfilter      Server list whitelist filter.  Format:\n"
           "                        [IP address 1];[IP address 2];[IP address 3]; ...\n"
           "  -F, --fastupdate      use 64 samples frame size mode\n"
           "  -l, --log             enable logging, set file name\n"
           "  -L, --licence         show an agreement window before users can connect\n"
           "  -m, --htmlstatus      enable HTML status file, set file name\n"
           "  -o, --serverinfo      registration info for this Server.  Format:\n"
           "                        [name];[city];[country as Qt5 QLocale ID]\n"
           "      --serverpublicip  public IP address for this Server.  Needed when\n"
           "                        registering with a server list hosted\n"
           "                        behind the same NAT\n"
           "  -P, --delaypan        start with delay panning enabled\n"
           "  -R, --recording       sets directory to contain recorded jams\n"
           "      --norecord        disables recording (when enabled by default by -R)\n"
           "  -s, --server          start Server\n"
           "      --serverbindip    IP address the Server will bind to (rather than all)\n"
           "  -T, --multithreading  use multithreading to make better use of\n"
           "                        multi-core CPUs and support more Clients\n"
           "  -u, --numchannels     maximum number of channels\n"
           "  -w, --welcomemessage  welcome message to display on connect\n"
           "                        (string or filename, HTML supported)\n"
           "  -z, --startminimized  start minimizied\n"
           "\n"
           "Client only:\n"
           "  -c, --connect         connect to given Server address on startup\n"
           "  -j, --nojackconnect   disable auto JACK connections\n"
           "  -M, --mutestream      starts the application in muted state\n"
           "      --mutemyown       mute me in my personal mix (headless only)\n"
           "      --clientname      Client name (window title and JACK client name)\n"
           "      --ctrlmidich      MIDI controller channel to listen\n"
           "\n"
           "Example: %1 -s --inifile myinifile.ini\n"
           "\n"
           "For more information and localized help see:\n"
           "https://jamulus.io/wiki/Command-Line-Options\n"
    ).arg( argv[0] );
    // clang-format on
}

bool GetFlagArgument ( char** argv, int& i, QString strShortOpt, QString strLongOpt )
{
    if ( ( !strShortOpt.compare ( argv[i] ) ) || ( !strLongOpt.compare ( argv[i] ) ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool GetStringArgument ( int argc, char** argv, int& i, QString strShortOpt, QString strLongOpt, QString& strArg )
{
    if ( ( !strShortOpt.compare ( argv[i] ) ) || ( !strLongOpt.compare ( argv[i] ) ) )
    {
        if ( ++i >= argc )
        {
            qCritical() << qUtf8Printable ( QString ( "%1: '%2' needs a string argument." ).arg ( argv[0] ).arg ( argv[i - 1] ) );
            exit ( 1 );
        }

        strArg = argv[i];

        return true;
    }
    else
    {
        return false;
    }
}

bool GetNumericArgument ( int     argc,
                          char**  argv,
                          int&    i,
                          QString strShortOpt,
                          QString strLongOpt,
                          double  rRangeStart,
                          double  rRangeStop,
                          double& rValue )
{
    if ( ( !strShortOpt.compare ( argv[i] ) ) || ( !strLongOpt.compare ( argv[i] ) ) )
    {
        QString errmsg = "%1: '%2' needs a numeric argument from '%3' to '%4'.";
        if ( ++i >= argc )
        {
            qCritical() << qUtf8Printable ( errmsg.arg ( argv[0] ).arg ( argv[i - 1] ).arg ( rRangeStart ).arg ( rRangeStop ) );
            exit ( 1 );
        }

        char* p;
        rValue = strtod ( argv[i], &p );
        if ( *p || ( rValue < rRangeStart ) || ( rValue > rRangeStop ) )
        {
            qCritical() << qUtf8Printable ( errmsg.arg ( argv[0] ).arg ( argv[i - 1] ).arg ( rRangeStart ).arg ( rRangeStop ) );
            exit ( 1 );
        }

        return true;
    }
    else
    {
        return false;
    }
}




