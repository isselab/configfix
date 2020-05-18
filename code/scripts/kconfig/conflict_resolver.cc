/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
 */
#include <iostream>
#include <QList>
#include "conflict_resolver.h"

QString tristate_value_to_string(tristate x)
{
    switch (x)
    {
        case no:
            return  QString::fromStdString("NO");
            break;
        case yes:
            return  QString::fromStdString("YES");
            break;
        case mod:
            return  QString::fromStdString("MODULE");
            break;

        default:
            break;
    }

}
tristate string_value_to_tristate(QString x){
    if (x == "YES"){
        return tristate::yes;
    } else if (x == "NO"){
        return tristate::no;
    } else if (x == "MODULE")
    {
        return tristate::mod;
    }

}
