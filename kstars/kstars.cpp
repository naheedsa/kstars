/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstars.h"

#include "config-kstars.h"
#include "version.h"

#include "fov.h"
#include "kactionmenu.h"
#include "kstarsadaptor.h"
#include "kstarsdata.h"
#include "kstarssplash.h"
#include "observinglist.h"
#include "Options.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "texturemanager.h"
#include "dialogs/finddialog.h"
#include "dialogs/exportimagedialog.h"
#include "skycomponents/starblockfactory.h"
#ifdef HAVE_INDI
#include "ekos/manager.h"
#include "indi/drivermanager.h"
#include "indi/guimanager.h"
#include "indi/indilistener.h"
#endif

#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsviewer.h"
#endif

#include <KActionCollection>
#include <KToolBar>

#ifdef Q_OS_WIN
#include <QProcess>
#endif
#include <QStatusBar>
#include <QMenu>

#include <kstars_debug.h>

KStars *KStars::pinstance = nullptr;
bool KStars::Closing = false;

KStars::KStars(bool doSplash, bool clockrun, const QString &startdate)
    : KXmlGuiWindow(), StartClockRunning(clockrun), StartDateString(startdate)
{
    // FIXME Hack to set RTL direction for Arabic
    // This is not a solution. It seems qtbase_ar.qm needs to take care of this?
    // qttranslations5-l10n does not contain qtbase_ar.qm
    // It seems qtbase_ar.ts does not exist for Qt 5.9 at all and needs to be translated.
    // https://wiki.qt.io/Qt_Localization
    if (i18n("Sky") == "السماء")
        qApp->setLayoutDirection(Qt::RightToLeft);

    setWindowTitle(i18nc("@title:window", "KStars"));

    // Set thread stack size to 32MB
    QThreadPool::globalInstance()->setStackSize(33554432);

    // Initialize logging settings
    if (Options::disableLogging())
        KSUtils::Logging::Disable();
    else if (Options::logToFile())
        KSUtils::Logging::UseFile();
    else
        KSUtils::Logging::UseDefault();

    KSUtils::Logging::SyncFilterRules();

    qCInfo(KSTARS) << "Welcome to KStars" << KSTARS_VERSION << KSTARS_BUILD_RELEASE;
    qCInfo(KSTARS) << "Build:" << KSTARS_BUILD_TS;
    qCInfo(KSTARS) << "OS:" << QSysInfo::productType();
    qCInfo(KSTARS) << "API:" << QSysInfo::buildAbi();
    qCInfo(KSTARS) << "Arch:" << QSysInfo::currentCpuArchitecture();
    qCInfo(KSTARS) << "Kernel Type:" << QSysInfo::kernelType();
    qCInfo(KSTARS) << "Kernel Version:" << QSysInfo::kernelVersion();
    qCInfo(KSTARS) << "Qt Version:" << QT_VERSION_STR;

    new KstarsAdaptor(
        this); // NOTE the weird case convention, which cannot be changed as the file is generated by the moc.

#ifdef Q_OS_OSX

    QString vlcPlugins = QDir(QCoreApplication::applicationDirPath() + "/../PlugIns/vlc").absolutePath();
    qputenv("VLC_PLUGIN_PATH", vlcPlugins.toLatin1());
    QString phonon_backend_path = QDir(QCoreApplication::applicationDirPath() +
                                       "/../PlugIns/phonon4qt5_backend/phonon_vlc.so").absolutePath();
    qputenv("PHONON_BACKEND", phonon_backend_path.toLatin1());

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path            = env.value("PATH", "");
    env.insert("PATH", "/usr/bin:/usr/local/bin:\"" + QCoreApplication::applicationDirPath() + "\":" + path);

    QProcess dbusCheck;
    dbusCheck.setProcessEnvironment(env);

    QString pluginsDir = QDir(QCoreApplication::applicationDirPath() + "/../PlugIns").absolutePath();
    QString dbusPlist  = pluginsDir + "/dbus/org.freedesktop.dbus-kstars.plist";
    QFile file(dbusPlist);
    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream in(&file);
        QString pListText = in.readAll();
        file.close();
        int programArgsLeft         = pListText.indexOf("<key>ProgramArguments</key>");
        int programArgsRight        = pListText.indexOf("</array>", programArgsLeft) + 8 - programArgsLeft;
        QString currentProgramArgs  = pListText.mid(programArgsLeft, programArgsRight);
        QString newProgramArguments = ""
                                      "<key>ProgramArguments</key>\n"
                                      "    <array>\n"
                                      "        <string>" +
                                      QCoreApplication::applicationDirPath() +
                                      "/dbus-daemon</string>\n"
                                      "        <string>--nofork</string>\n"
                                      "        <string>--config-file=" +
                                      pluginsDir +
                                      "/dbus/kstars.conf</string>\n"
                                      "    </array>";
        pListText.replace(currentProgramArgs, newProgramArguments);
        if (file.open(QIODevice::WriteOnly))
        {
            QTextStream stream(&file);
            stream << pListText;
            file.close();

            dbusCheck.start("chmod 775 " + dbusPlist);
            dbusCheck.waitForFinished();
            dbusCheck.start("launchctl load -w \"" + dbusPlist + "\"");
            dbusCheck.waitForFinished();
            qDebug("Starting DBus");
        }
        else
        {
            qDebug("DBus File Write Error");
        }
    }
    else
    {
        qDebug("DBus File Read Error");
    }
#endif

    QDBusConnection::sessionBus().registerObject("/KStars", this);
    QDBusConnection::sessionBus().registerService("org.kde.kstars");

#ifdef HAVE_CFITSIO
    m_GenericFITSViewer.clear();
#endif

    // Set pinstance to yourself
    pinstance = this;

    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(slotAboutToQuit()));

    //Initialize QActionGroups
    projectionGroup = new QActionGroup(this);
    cschemeGroup    = new QActionGroup(this);
    hipsGroup       = new QActionGroup(this);
    telescopeGroup  = new QActionGroup(this);
    telescopeGroup->setExclusive(false);
    domeGroup       = new QActionGroup(this);
    domeGroup->setExclusive(false);
    skymapOrientationGroup = new QActionGroup(this);


    m_KStarsData = KStarsData::Create();
    Q_ASSERT(m_KStarsData);
    //Set Geographic Location from Options
    m_KStarsData->setLocationFromOptions();

    //Initialize Time and Date
    bool datetimeSet = false;
    if (StartDateString.isEmpty() == false)
    {
        KStarsDateTime startDate = KStarsDateTime::fromString(StartDateString);
        if (startDate.isValid())
            data()->changeDateTime(data()->geo()->LTtoUT(startDate));
        else
            data()->changeDateTime(KStarsDateTime::currentDateTimeUtc());

        datetimeSet = true;
    }
    // JM 2016-11-15: Not need to set it again as it was initialized in the ctor of SimClock
    /*
    else
        data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );
    */

    // If we are starting paused (--paused is not in the command line) change datetime in data
    if (StartClockRunning == false)
    {
        qCInfo(KSTARS) << "KStars is started in paused state.";
        if (datetimeSet == false)
            data()->changeDateTime(KStarsDateTime::currentDateTimeUtc());
    }

    // Setup splash screen
    KStarsSplash *splash = nullptr;
    if (doSplash)
    {
        splash = new KStarsSplash(nullptr);
        connect(m_KStarsData, SIGNAL(progressText(QString)), splash, SLOT(setMessage(QString)));
        splash->show();
    }
    else
    {
        connect(m_KStarsData, SIGNAL(progressText(QString)), m_KStarsData, SLOT(slotConsoleMessage(QString)));
    }

    /*
    //set up Dark color scheme for application windows
    DarkPalette = QPalette(QColor("black"), QColor("black"));
    DarkPalette.setColor(QPalette::Inactive, QPalette::WindowText, QColor("red"));
    DarkPalette.setColor(QPalette::Normal, QPalette::WindowText, QColor("red"));
    DarkPalette.setColor(QPalette::Normal, QPalette::Base, QColor("black"));
    DarkPalette.setColor(QPalette::Normal, QPalette::Text, QColor(238, 0, 0));
    DarkPalette.setColor(QPalette::Normal, QPalette::Highlight, QColor(238, 0, 0));
    DarkPalette.setColor(QPalette::Normal, QPalette::HighlightedText, QColor("black"));
    DarkPalette.setColor(QPalette::Inactive, QPalette::Text, QColor(238, 0, 0));
    DarkPalette.setColor(QPalette::Inactive, QPalette::Base, QColor(30, 10, 10));
    //store original color scheme
    OriginalPalette = QApplication::palette();
    */

    //Initialize data.  When initialization is complete, it will run dataInitFinished()
    if (!m_KStarsData->initialize())
        return;
    delete splash;
    datainitFinished();
}

KStars *KStars::createInstance(bool doSplash, bool clockrun, const QString &startdate)
{
    delete pinstance;
    // pinstance is set directly in constructor.
    new KStars(doSplash, clockrun, startdate);
    Q_ASSERT(pinstance && "pinstance must be non NULL");
    return pinstance;
}

KStars::~KStars()
{
    releaseResources();
    Q_ASSERT(pinstance);
    pinstance = nullptr;
#ifdef PROFILE_COORDINATE_CONVERSION
    qDebug() << Q_FUNC_INFO << "Spent " << SkyPoint::cpuTime_EqToHz << " seconds in " << SkyPoint::eqToHzCalls
             << " calls to SkyPoint::EquatorialToHorizontal, for an average of "
             << 1000. * (SkyPoint::cpuTime_EqToHz / SkyPoint::eqToHzCalls) << " ms per call";
#endif

#ifdef COUNT_DMS_SINCOS_CALLS
    qDebug() << Q_FUNC_INFO << "Constructed " << dms::dms_constructor_calls << " dms objects, of which " <<
             dms::dms_with_sincos_called
             << " had trigonometric functions called on them = "
             << (float(dms::dms_with_sincos_called) / float(dms::dms_constructor_calls)) * 100. << "%";
    qDebug() << Q_FUNC_INFO << "Of the " << dms::trig_function_calls << " calls to sin/cos/sincos on dms objects, "
             << dms::redundant_trig_function_calls << " were redundant = "
             << ((float(dms::redundant_trig_function_calls) / float(dms::trig_function_calls)) * 100.) << "%";
    qDebug() << Q_FUNC_INFO << "We had " << CachingDms::cachingdms_bad_uses << " bad uses of CachingDms in all, compared to "
             << CachingDms::cachingdms_constructor_calls << " constructed CachingDms objects = "
             << (float(CachingDms::cachingdms_bad_uses) / float(CachingDms::cachingdms_constructor_calls)) * 100.
             << "% bad uses";
#endif

    /* BUG 366596: Some KDE applications processes remain as background (zombie) processes after closing
         * No solution to this bug so far using Qt 5.8 as of 2016-11-24
         * Therefore, the only way to solve this on Windows is to explicitly kill kstars.exe
         * Hopefully we do not need this hack once the above bug is resolved.
         */
#ifdef Q_OS_WIN
    QProcess::execute("taskkill /im kstars.exe /f");
#endif
}

void KStars::releaseResources()
{
    delete m_KStarsData;
    m_KStarsData = nullptr;
    delete StarBlockFactory::Instance();
    TextureManager::Release();
    SkyQPainter::releaseImageCache();
    FOVManager::releaseCache();

#ifdef HAVE_INDI
    GUIManager::release();
    Ekos::Manager::release();
    DriverManager::release();
#endif

    QSqlDatabase::removeDatabase("userdb");
    QSqlDatabase::removeDatabase("skydb");
}

void KStars::clearCachedFindDialog()
{
#if 0
    if (m_FindDialog) // dialog is cached
    {
        /** Delete findDialog only if it is not opened */
        if (m_FindDialog->isHidden())
        {
            delete m_FindDialog;
            m_FindDialog     = nullptr;
            DialogIsObsolete = false;
        }
        else
            DialogIsObsolete = true; // dialog was opened so it could not deleted
    }
#endif
}

void KStars::applyConfig(bool doApplyFocus)
{
    if (Options::isTracking())
    {
        actionCollection()->action("track_object")->setText(i18n("Stop &Tracking"));
        actionCollection()
        ->action("track_object")
        ->setIcon(QIcon::fromTheme("document-encrypt"));
    }

    actionCollection()
    ->action("coordsys")
    ->setText(Options::useAltAz() ? i18n("Switch to Star Globe View (Equatorial &Coordinates)") :
              i18n("Switch to Horizontal View (Horizontal &Coordinates)"));

    actionCollection()->action("show_time_box")->setChecked(Options::showTimeBox());
    actionCollection()->action("show_location_box")->setChecked(Options::showGeoBox());
    actionCollection()->action("show_focus_box")->setChecked(Options::showFocusBox());
    actionCollection()->action("show_statusBar")->setChecked(Options::showStatusBar());
    actionCollection()->action("show_sbAzAlt")->setChecked(Options::showAltAzField());
    actionCollection()->action("show_sbRADec")->setChecked(Options::showRADecField());
    actionCollection()->action("show_sbJ2000RADec")->setChecked(Options::showJ2000RADecField());
    actionCollection()->action("show_stars")->setChecked(Options::showStars());
    actionCollection()->action("show_deepsky")->setChecked(Options::showDeepSky());
    actionCollection()->action("show_planets")->setChecked(Options::showSolarSystem());
    actionCollection()->action("show_clines")->setChecked(Options::showCLines());
    actionCollection()->action("show_constellationart")->setChecked(Options::showConstellationArt());
    actionCollection()->action("show_cnames")->setChecked(Options::showCNames());
    actionCollection()->action("show_cbounds")->setChecked(Options::showCBounds());
    actionCollection()->action("show_mw")->setChecked(Options::showMilkyWay());
    actionCollection()->action("show_equatorial_grid")->setChecked(Options::showEquatorialGrid());
    actionCollection()->action("show_horizontal_grid")->setChecked(Options::showHorizontalGrid());
    actionCollection()->action("show_horizon")->setChecked(Options::showGround());
    actionCollection()->action("show_flags")->setChecked(Options::showFlags());
    actionCollection()->action("show_supernovae")->setChecked(Options::showSupernovae());
    actionCollection()->action("show_satellites")->setChecked(Options::showSatellites());
    actionCollection()->action("erect_observer_correction")->setChecked(Options::erectObserverCorrection());
    actionCollection()->action("erect_observer_correction")->setEnabled(Options::useAltAz());
    statusBar()->setVisible(Options::showStatusBar());

    //color scheme
    m_KStarsData->colorScheme()->loadFromConfig();
    //QApplication::setPalette(Options::darkAppColors() ? DarkPalette : OriginalPalette);
    /**
    //Note:  This uses style sheets to set the dark colors, this should be cross platform.  Palettes have a different behavior on OS X and Windows as opposed to Linux.
    //It might be a good idea to use stylesheets in the future instead of palettes but this will work for now for OS X.
    //This is also in KStarsDbus.cpp.  If you change it, change it in BOTH places.
    @code
    #ifdef Q_OS_OSX
        if (Options::darkAppColors())
            qApp->setStyleSheet(
                "QWidget { background-color: black; color:red; "
                "selection-background-color:rgb(30,30,30);selection-color:white}"
                "QToolBar { border:none }"
                "QTabBar::tab:selected { background-color:rgb(50,50,50) }"
                "QTabBar::tab:!selected { background-color:rgb(30,30,30) }"
                "QPushButton { background-color:rgb(50,50,50);border-width:1px; border-style:solid;border-color:black}"
                "QPushButton::disabled { background-color:rgb(10,10,10);border-width:1px; "
                "border-style:solid;border-color:black }"
                "QToolButton:Checked { background-color:rgb(30,30,30); border:none }"
                "QComboBox { background-color:rgb(30,30,30); }"
                "QComboBox::disabled { background-color:rgb(10,10,10) }"
                "QScrollBar::handle { background: rgb(30,30,30) }"
                "QSpinBox { border-width: 1px; border-style:solid; border-color:rgb(30,30,30) }"
                "QDoubleSpinBox { border-width:1px; border-style:solid; border-color:rgb(30,30,30) }"
                "QLineEdit { border-width: 1px; border-style: solid; border-color:rgb(30,30,30) }"
                "QCheckBox::indicator:unchecked { background-color:rgb(30,30,30);border-width:1px; "
                "border-style:solid;border-color:black }"
                "QCheckBox::indicator:checked { background-color:red;border-width:1px; "
                "border-style:solid;border-color:black }"
                "QRadioButton::indicator:unchecked { background-color:rgb(30,30,30) }"
                "QRadioButton::indicator:checked { background-color:red }"
                "QRoundProgressBar { alternate-background-color:black }"
                "QDateTimeEdit {background-color:rgb(30,30,30); border-width: 1px; border-style:solid; "
                "border-color:rgb(30,30,30) }"
                "QHeaderView { color:red;background-color:black }"
                "QHeaderView::Section { background-color:rgb(30,30,30) }"
                "QTableCornerButton::section{ background-color:rgb(30,30,30) }"
                "");
        else
            qApp->setStyleSheet("");
    #endif
    @endcode
    **/

    //Set toolbar options from config file
    toolBar("kstarsToolBar")->applySettings(KSharedConfig::openConfig()->group("MainToolBar"));
    toolBar("viewToolBar")->applySettings(KSharedConfig::openConfig()->group("ViewToolBar"));

    //Geographic location
    data()->setLocationFromOptions();

    //Focus
    if (doApplyFocus)
    {
        SkyObject *fo = data()->objectNamed(Options::focusObject());
        if (fo && fo != map()->focusObject())
        {
            map()->setClickedObject(fo);
            map()->setClickedPoint(fo);
            map()->slotCenter();
        }

        if (!fo)
        {
            SkyPoint fp(Options::focusRA(), Options::focusDec());
            if (fp.ra().Degrees() != map()->focus()->ra().Degrees() ||
                    fp.dec().Degrees() != map()->focus()->dec().Degrees())
            {
                map()->setClickedPoint(&fp);
                map()->slotCenter();
            }
        }
    }
}

void KStars::showImgExportDialog()
{
    if (m_ExportImageDialog)
        m_ExportImageDialog->show();
}

void KStars::syncFOVActions()
{
    foreach (QAction *action, fovActionMenu->menu()->actions())
    {
        if (action->text().isEmpty())
        {
            continue;
        }

        if (Options::fOVNames().contains(action->text().remove(0, 1)))
        {
            action->setChecked(true);
        }
        else
        {
            action->setChecked(false);
        }
    }
}

void KStars::hideAllFovExceptFirst()
{
    // When there is only one visible FOV symbol, we don't need to do anything
    // Also, don't do anything if there are no available FOV symbols.
    if (data()->visibleFOVs.size() == 1 || data()->availFOVs.isEmpty())
    {
        return;
    }
    else
    {
        // If there are no visible FOVs, select first available
        if (data()->visibleFOVs.isEmpty())
        {
            Q_ASSERT(!data()->availFOVs.isEmpty());
            Options::setFOVNames(QStringList(data()->availFOVs.first()->name()));
        }
        else
        {
            Options::setFOVNames(QStringList(data()->visibleFOVs.first()->name()));
        }

        // Sync FOV and update skymap
        data()->syncFOV();
        syncFOVActions();
        map()->update(); // SkyMap::forceUpdate() is not required, as FOVs are drawn as overlays
    }
}

void KStars::selectNextFov()
{
    if (data()->getVisibleFOVs().isEmpty())
        return;

    Q_ASSERT(!data()
             ->getAvailableFOVs()
             .isEmpty()); // The available FOVs had better not be empty if the visible ones are not.

    FOV *currentFov = data()->getVisibleFOVs().first();
    int currentIdx  = data()->availFOVs.indexOf(currentFov);

    // If current FOV is not the available FOV list or there is only 1 FOV available, then return
    if (currentIdx == -1 || data()->availFOVs.size() < 2)
    {
        return;
    }

    QStringList nextFovName;
    if (currentIdx == data()->availFOVs.size() - 1)
    {
        nextFovName << data()->availFOVs.first()->name();
    }
    else
    {
        nextFovName << data()->availFOVs.at(currentIdx + 1)->name();
    }

    Options::setFOVNames(nextFovName);
    data()->syncFOV();
    syncFOVActions();
    map()->update();
}

void KStars::selectPreviousFov()
{
    if (data()->getVisibleFOVs().isEmpty())
        return;

    Q_ASSERT(!data()
             ->getAvailableFOVs()
             .isEmpty()); // The available FOVs had better not be empty if the visible ones are not.

    FOV *currentFov = data()->getVisibleFOVs().first();
    int currentIdx  = data()->availFOVs.indexOf(currentFov);

    // If current FOV is not the available FOV list or there is only 1 FOV available, then return
    if (currentIdx == -1 || data()->availFOVs.size() < 2)
    {
        return;
    }

    QStringList prevFovName;
    if (currentIdx == 0)
    {
        prevFovName << data()->availFOVs.last()->name();
    }
    else
    {
        prevFovName << data()->availFOVs.at(currentIdx - 1)->name();
    }

    Options::setFOVNames(prevFovName);
    data()->syncFOV();
    syncFOVActions();
    map()->update();
}

//FIXME Port to QML2
//#if 0
void KStars::showWISettingsUI()
{
    slotWISettings();
}
//#endif

void KStars::updateTime(const bool automaticDSTchange)
{
    // Due to frequently use of this function save data and map pointers for speedup.
    // Save options and geo() to a pointer would not speedup because most of time options
    // and geo will accessed only one time.
    KStarsData *Data = data();
    // dms oldLST( Data->lst()->Degrees() );

    Data->updateTime(Data->geo(), automaticDSTchange);

    //We do this outside of kstarsdata just to get the coordinates
    //displayed in the infobox to update every second.
    //	if ( !Options::isTracking() && LST()->Degrees() > oldLST.Degrees() ) {
    //		int nSec = int( 3600.*( LST()->Hours() - oldLST.Hours() ) );
    //		Map->focus()->setRA( Map->focus()->ra().Hours() + double( nSec )/3600. );
    //		if ( Options::useAltAz() ) Map->focus()->EquatorialToHorizontal( LST(), geo()->lat() );
    //		Map->showFocusCoords();
    //	}

    //If time is accelerated beyond slewTimescale, then the clock's timer is stopped,
    //so that it can be ticked manually after each update, in order to make each time
    //step exactly equal to the timeScale setting.
    //Wrap the call to manualTick() in a singleshot timer so that it doesn't get called until
    //the skymap has been completely updated.
    if (Data->clock()->isManualMode() && Data->clock()->isActive())
    {
        // Jasem 2017-11-13: Time for each update varies.
        // Ideally we want to advance the simulation clock by
        // the current clock scale (e.g. 1 hour) every 1 second
        // of real time. However, the sky map update, depending on calculations and
        // drawing of objects, takes variable time to complete.
        //QTimer::singleShot(0, Data->clock(), SLOT(manualTick()));
        QTimer::singleShot(1000, Data->clock(), SLOT(manualTick()));
    }
}

#ifdef HAVE_CFITSIO
const QSharedPointer<FITSViewer> &KStars::createFITSViewer()
{
    if (Options::singleWindowCapturedFITS())
        return KStars::Instance()->genericFITSViewer();
    else
    {
        QSharedPointer<FITSViewer> newFITSViewer(new FITSViewer(Options::independentWindowFITS() ? nullptr : KStars::Instance()));
        m_FITSViewers.append(newFITSViewer);
        connect(newFITSViewer.get(), &FITSViewer::terminated, this, [ & ]()
        {
            m_FITSViewers.removeOne(newFITSViewer);
        });
        return m_FITSViewers.constLast();
    }
}

const QSharedPointer<FITSViewer> &KStars::genericFITSViewer()
{
    if (m_GenericFITSViewer.isNull())
    {
        m_GenericFITSViewer.reset(new FITSViewer(Options::independentWindowFITS() ? nullptr : this));
        connect(m_GenericFITSViewer.get(), &FITSViewer::terminated, this, [this]()
        {
            m_FITSViewers.removeOne(m_GenericFITSViewer);
            m_GenericFITSViewer.clear();
        });
        m_FITSViewers.append(m_GenericFITSViewer);
    }

    return m_GenericFITSViewer;
}

void KStars::clearAllViewers()
{
    for (auto &fv : m_FITSViewers)
        fv->close();

    m_FITSViewers.clear();
}
#endif

void KStars::closeEvent(QCloseEvent *event)
{
    KStars::Closing = true;
    QWidget::closeEvent(event);
    slotAboutToQuit();
}
