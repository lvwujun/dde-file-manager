/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     huanyu<huanyu@uniontech.com>
 *
 * Maintainer: huanyu<huanyu@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "desktopdbusinterface.h"

#include "config.h"   //cmake
#include "tools/upgrade/builtininterface.h"

#include <DApplication>
#include <DMainWindow>

#include <QMainWindow>
#include <QWidget>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QtGlobal>
#include <QDBusInterface>
#include <QProcess>

#include <dfm-framework/dpf.h>

#include <iostream>
#include <algorithm>
#include <unistd.h>

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE
using namespace dde_desktop;

#ifdef DFM_ORGANIZATION_NAME
#    define ORGANIZATION_NAME DFM_ORGANIZATION_NAME
#else
#    define ORGANIZATION_NAME "deepin"
#endif

#define BUILD_VERSION ((QString(VERSION) == "") ? "6.0.0.0" : QString(VERSION))

/// @brief PLUGIN_INTERFACE 默认插件iid
static const char *const kDesktopPluginInterface = "org.deepin.plugin.desktop";
static const char *const kCommonPluginInterface = "org.deepin.plugin.common";
static const char *const kPluginCore = "ddplugin-core";
static const char *const kLibCore = "libddplugin-core.so";

static bool pluginsLoad()
{
    dpfCheckTimeBegin();

    QString pluginsDir(qApp->applicationDirPath() + "/../../plugins");
    QStringList pluginsDirs;
    if (!QDir(pluginsDir).exists()) {
        qInfo() << QString("Path does not exist, use path : %1").arg(DFM_PLUGIN_COMMON_CORE_DIR);
        pluginsDirs << QString(DFM_PLUGIN_COMMON_CORE_DIR)
                    << QString(DFM_PLUGIN_DESKTOP_CORE_DIR)
                    << QString(DFM_PLUGIN_COMMON_EDGE_DIR)
                    << QString(DFM_PLUGIN_DESKTOP_EDGE_DIR);
    } else {
        pluginsDirs.push_back(pluginsDir + "/desktop");
        pluginsDirs.push_back(pluginsDir + "/common");
        pluginsDirs.push_back(pluginsDir);
    }
    qDebug() << "using plugins dir:" << pluginsDirs;
    DPF_NAMESPACE::LifeCycle::initialize({ kDesktopPluginInterface, kCommonPluginInterface }, pluginsDirs);

    qInfo() << "Depend library paths:" << DApplication::libraryPaths();
    qInfo() << "Load plugin paths: " << dpf::LifeCycle::pluginPaths();

    // read all plugins in setting paths
    if (!DPF_NAMESPACE::LifeCycle::readPlugins())
        return false;

    // We should make sure that the core plugin is loaded first
    auto corePlugin = DPF_NAMESPACE::LifeCycle::pluginMetaObj(kPluginCore);
    if (corePlugin.isNull())
        return false;
    if (!corePlugin->fileName().contains(kLibCore)) {
        qWarning() << corePlugin->fileName() << "is not" << kLibCore;
        return false;
    }
    if (!DPF_NAMESPACE::LifeCycle::loadPlugin(corePlugin))
        return false;

    // load plugins without core
    if (!DPF_NAMESPACE::LifeCycle::loadPlugins())
        return false;

    dpfCheckTimeEnd();

    return true;
}

static void registerDDESession()
{
    const char *envName = "DDE_SESSION_PROCESS_COOKIE_ID";
    QByteArray cookie = qgetenv(envName);
    qunsetenv(envName);

    if (!cookie.isEmpty()) {
        QDBusInterface iface("com.deepin.SessionManager",
                             "/com/deepin/SessionManager",
                             "com.deepin.SessionManager",
                             QDBusConnection::sessionBus());
        iface.call("Register", QString(cookie));
    }
}

static void initLog()
{
    const QString logFormat = "%{time}{yyyyMMdd.HH:mm:ss.zzz}[%{type:1}][%{function:-35} %{line:-4} %{threadid} ] %{message}\n";
    dpfLogManager->setLogFormat(logFormat);
    dpfLogManager->registerConsoleAppender();
    dpfLogManager->registerFileAppender();
}

static void checkUpgrade(DApplication *app)
{
    if (!dfm_upgrade::isNeedUpgrade())
        return;

    qInfo() << "try to upgrade in desktop";
    QMap<QString, QString> args;
    args.insert("version", app->applicationVersion());
    args.insert(dfm_upgrade::kArgDesktop, "dde-desktop");

    QString lib;
    GetUpgradeLibraryPath(lib);

    int ret = dfm_upgrade::tryUpgrade(lib, args);
    if (ret < 0) {
        qWarning() << "something error, exit current process." << app->applicationPid();
        _Exit(-1);
    } else if (ret == 0) {
        auto args = app->arguments();
        // remove first
        if (!args.isEmpty())
            args.pop_front();

        QDBusConnection::sessionBus().unregisterService(kDesktopServiceName);
        qInfo() << "restart self " << app->applicationFilePath() << args;
        QProcess::startDetached(app->applicationFilePath(), args);
        _Exit(-1);
    }

    return;
}

int main(int argc, char *argv[])
{
    DApplication a(argc, argv);
    a.setOrganizationName(ORGANIZATION_NAME);
    a.setApplicationDisplayName(a.translate("DesktopMain", "Desktop"));
    a.setApplicationVersion(BUILD_VERSION);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    {
        // load translation
        QString appName = a.applicationName();
        a.setApplicationName("dde-file-manager");
        a.loadTranslator();
        a.setApplicationName(appName);
    }

    DPF_NAMESPACE::backtrace::installStackTraceHandler();
    initLog();

    qInfo() << "start desktop " << a.applicationVersion() << "pid" << getpid() << "parent id" << getppid()
            << "argments" << a.arguments();

    // if (!fileDialogOnly)
    if (true) {
        QDBusConnection conn = QDBusConnection::sessionBus();

        if (!conn.registerService(kDesktopServiceName)) {
            qCritical() << "registerService Failed, maybe service exist" << conn.lastError();
            exit(0x0002);
        }

        DesktopDBusInterface *interface = new DesktopDBusInterface(&a);
        auto registerOptions = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties;
        if (!conn.registerObject(kDesktopServicePath, kDesktopServiceInterface, interface, registerOptions)) {
            qCritical() << "registerObject Failed" << conn.lastError();
            exit(0x0003);
        }

        // Notify dde-desktop start up
        registerDDESession();
    }

    checkUpgrade(&a);

    if (!pluginsLoad()) {
        qCritical() << "Load pugin failed!";
        abort();
    }

    int ret { a.exec() };
    DPF_NAMESPACE::LifeCycle::shutdownPlugins();
    return ret;
}
