/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangyu<zhangyub@uniontech.com>
 *
 * Maintainer: zhangyu<zhangyub@uniontech.com>
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

#include "upgradeinterface.h"
#include "core/upgradelocker.h"
#include "core/upgradefactory.h"
#include "dialog/processdialog.h"
#include "units/unitlist.h"

#include "builtininterface.h"

#include "stubext.h"

#include <gtest/gtest.h>

using namespace testing;
using namespace dfm_upgrade;

class DoUpgradeTest : public Test
{
public:
    void SetUp() override {
        system("touch /var/tmp/dfm-upgraded.lock");
        stub.set_lamda(&upgradeConfigDir, [](){
            return QString("/var/tmp");
        });

        stub.set_lamda(&createUnits, [](){
            return QList<QSharedPointer<UpgradeUnit>>();
        });

    }
    void TearDown() override {
        system("rm /var/tmp/dfm-upgraded.lock");
        stub.clear();
    }

    stub_ext::StubExt stub;
};


TEST_F(DoUpgradeTest, empty_args)
{
    EXPECT_EQ(-1, dfm_tools_upgrade_doUpgrade({}));
    EXPECT_TRUE(QFile::exists("/var/tmp/dfm-upgraded.lock"));
}

TEST_F(DoUpgradeTest, locked)
{
    stub_ext::StubExt stub;
    stub.set_lamda(&UpgradeLocker::isLock, [](){
        return true;
    });

    QMap<QString, QString> args;
    args.insert(kArgDesktop, "6.0.0");
    EXPECT_EQ(-1, dfm_tools_upgrade_doUpgrade(args));
    EXPECT_TRUE(QFile::exists("/var/tmp/dfm-upgraded.lock"));
}

TEST_F(DoUpgradeTest, noneed)
{
    stub_ext::StubExt stub;
    stub.set_lamda(&UpgradeLocker::isLock, [](){
        return false;
    });

    stub.set_lamda(&isNeedUpgrade, [](){
        return false;
    });

    QMap<QString, QString> args;
    args.insert(kArgDesktop, "6.0.0");
    EXPECT_EQ(-1, dfm_tools_upgrade_doUpgrade(args));
    EXPECT_TRUE(QFile::exists("/var/tmp/dfm-upgraded.lock"));
}


TEST_F(DoUpgradeTest, reject)
{
    stub_ext::StubExt stub;
    stub.set_lamda(&UpgradeLocker::isLock, [](){
        return false;
    });

    stub.set_lamda(&ProcessDialog::execDialog, [](){
        return false;
    });

    QMap<QString, QString> args;
    args.insert(kArgDesktop, "6.0.0");
    EXPECT_EQ(-1, dfm_tools_upgrade_doUpgrade(args));
    EXPECT_TRUE(QFile::exists("/var/tmp/dfm-upgraded.lock"));
}

TEST_F(DoUpgradeTest, accept)
{
    stub_ext::StubExt stub;
    stub.set_lamda(&UpgradeLocker::isLock, [](){
        return false;
    });

    stub.set_lamda(&ProcessDialog::execDialog, [](){
        return true;
    });

    QMap<QString, QString> args;
    args.insert(kArgDesktop, "6.0.0");
    EXPECT_EQ(0, dfm_tools_upgrade_doUpgrade(args));
    EXPECT_FALSE(QFile::exists("/var/tmp/dfm-upgraded.lock"));
}
