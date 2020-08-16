#pragma once

#include <Arduino.h>

String minutes_to_time(int mins)
{
    int hrs = mins / 60;
    int leftover_mins = mins - hrs * 60;

    String result = String(hrs) + ":" + String(leftover_mins);

    if (leftover_mins < 10)
    {
        result += "0";
    }

    if (hrs < 10)
    {
        result = "0" + result;
    }

    return result;
}

String minutes_to_duration(int mins)
{
    if (mins == 0)
    {
        return "instant";
    }

    String result = "";

    int hrs = mins / 60;
    int leftover_mins = mins - hrs * 60;

    if (hrs > 1)
    {
        result += String(hrs) + "hrs";
    }
    else if (hrs != 0)
    {
        result += String(hrs) + "hr";
    }

    if (hrs && leftover_mins)
    {
        result += " ";
    }

    if (leftover_mins > 0)
    {
        result += String(leftover_mins) + " mins";
    }

    return result;
}

String left_pad(int v)
{
    if (v < 10)
    {
        return "0" + String(v);
    }

    return String(v);
}
