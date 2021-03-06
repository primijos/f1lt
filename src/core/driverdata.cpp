/*******************************************************************************
 *                                                                             *
 *   F1LT - unofficial Formula 1 live timing application                       *
 *   Copyright (C) 2012-2013  Mariusz Pilarek (pieczaro@gmail.com)             *
 *                                                                             *
 *   This program is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by      *
 *   the Free Software Foundation, either version 3 of the License, or         *
 *   (at your option) any later version.                                       *
 *                                                                             *
 *   This program is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 *   GNU General Public License for more details.                              *
 *                                                                             *
 *   You should have received a copy of the GNU General Public License         *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                             *
 *******************************************************************************/


#include "driverdata.h"
#include "eventdata.h"
#include "seasondata.h"

#include <QDebug>
int DriverData::cnt = 0;
//==================================================================

DriverData &DriverData::operator=(const DriverData &dd)
{
    if (this == &dd)
        return *this;    

    carID = dd.carID;

    number = dd.number;
    pos = dd.pos;

    driver = dd.driver;

    numPits = dd.numPits;

    retired = dd.retired;

    pitData = dd.pitData;

    releasedFromPits = dd.releasedFromPits;

    colorData = dd.colorData;

    lapData.clear();
    for (int i = 0; i < dd.lapData.size(); ++i)
        lapData.append(dd.lapData[i]);

    lastLap = dd.lastLap; //&lapData[lapData.size()-1];
    sessionRecords = dd.sessionRecords;

    posHistory = dd.posHistory;

    for (int i = 0; i < 3; ++i)
        qualiTimes[i] = dd.qualiTimes[i];

    return *this;
}

//----------------------------------

//used in race - checks if the drivers numLap is correct according to the events total laps and gap to the leader
void DriverData::correctNumLap(int raceNumLap)
{
    //first of all check if we are lapped
    int lapped = 0;
    if (lastLap.gap!= "" && lastLap.gap[lastLap.gap.size()-1] == 'L')
        lapped = lastLap.gap.left(lastLap.gap.size()-1).toInt();

    //now correct the lap number, raceNumLap is obtained from the leaders interval info - that's the only way here to get it
    if (lastLap.lapNum + lapped != raceNumLap)
        lastLap.lapNum = raceNumLap - lapped;

    //sometimes, when the driver is close to being lapped server doesn't send his gap data,
    //so if the lastLap.numLap is greater by 2 or more laps than lapData.last().numLap - we have to correct it...
    if (lastLap.gap == "" && !lapData.isEmpty() && lastLap.lapNum > lapData.last().lapNum+1)
        lastLap.lapNum = lapData.last().lapNum+1;
}

//during Q1 when the drivers time is worse than 107% lt server doesn't update his position, we have to do this manually
void DriverData::correctPosition(const EventData &ed)
{
    if (ed.getQualiPeriod() == 1 && ed.getSessionRecords().getFastestLap().getTime().isValid() &&
        ((ed.getSessionRecords().getFastestLap().getTime().calc107p() < qualiTimes[0]) ||
         !qualiTimes[0].isValid()))
    {
        int position = ed.correctPosition(qualiTimes[0]);

        if (position >= 1)
        {
            pos = position;
            lastLap.pos = position;
            sessionRecords.bestQLaps[0].pos = position;

            int idx = lapData.indexOf(sessionRecords.getBestQualiLap(1));
            if (idx >= 0)
                lapData[idx].pos = position;
        }
//        colorData.positionColor() = LTPackets::RED;
    }
}

void DriverData::addLap(const EventData &ed)
{    
    if (ed.getEventType() == LTPackets::RACE_EVENT)
    {
        addRaceLap(ed);
    }
    else
    {
        addFPQLap(ed);
    }
}

void DriverData::addRaceLap(const EventData &ed)
{
    //ok, this looks a bit complicated, but since the LT server doesn't give us the actuall lap number for every driver during the race we have to find another way to gather laps:
    //- first of all - don't append laps if driver has retired (it's rather obvious)
    //- don't add empty lap time, except for the first lap of the race - it's always empty
    //- if this lap is in-lap - add it, if no - add it only if we have the sector 3 time
    //- if the lapData array is empty - check if lapNum is greater than 0
    //- don't add the out lap - "OUT",
    //- finally - check if we don't try to add the same lap again, we use the gap, interval and lap time info for this
    if (!retired && ed.getCompletedLaps() > 0 && (lastLap.lapTime.toString() != "" || lastLap.lapNum == 1) &&
//        ((lastLap.lapTime.toString() != "IN PIT" && lastLap.sector3.toString() != "") || lastLap.lapTime.toString() == "IN PIT") &&
         ((lastLap.lapNum > 0 && lapData.isEmpty()) ||
         (!lapData.isEmpty() &&
//          (lastLap.numLap > lapData.last().numLap) &&
          (lastLap.lapTime.toString() != "OUT" /*&& !(lastLap.sector3.toString() == "STOP" && lapData.last().sector3.toString() == "STOP")*/) &&
          !(lapData.last().gap == lastLap.gap && lapData.last().interval == lastLap.interval && lapData.last().lapTime == lastLap.lapTime)
           )))
    {
        //this is tricky - if driver goes to the pits, we get this info before he crosses the finish line, but not always...
        //therefore, we don't correct the lap number, assuming that everything is ok, and the lap number is last lap + 1
        if ((lastLap.lapTime.toString() != "IN PIT" && !lapData.isEmpty()) || (lapData.isEmpty() && lastLap.lapTime.toString() == "OUT"))
            correctNumLap(ed.getCompletedLaps());

        //1st lap is always empty (excluding situations when driver goes to pit or retires), so if we got a valid time on the first lap
        //it means that LT server has sent us some junk (probably time from quali)
        if (lastLap.lapNum == 1 && lastLap.lapTime.isValid())
            lastLap.lapTime = LapTime();

        //if this is RETIRED lap, update only the lap time
        if (lastLap.lapTime.toString() == "RETIRED" && !lapData.isEmpty())
        {
            lapData.last().lapTime = lastLap.lapTime;
            return;
        }

        if (lastLap.lapTime.toString() == "IN PIT")
            lastLap.raceLapExtraData.pitLap = true;
        else
            lastLap.raceLapExtraData.pitLap = false;

        //if we get "IN PIT" before driver crossed the line, we get it again after he croses, in that case update only gap and interval
        if (!lapData.empty() && lastLap.lapTime.toString() == "IN PIT" && lapData.last().lapTime.toString() == "IN PIT" && !releasedFromPits)
        {
            lapData.last().gap = lastLap.gap;
            lapData.last().interval = lastLap.interval;
            return;
        }
        //when connected to LT during the race and driver was going out of the pits we save this lap as PIT lap
        if (lastLap.lapTime.toString() == "OUT")
            lastLap.lapTime = LapTime("IN PIT");

        lastLap.carID = carID;

        if (!lapData.empty() && lapData.last().lapNum >= lastLap.lapNum)
            lapData.last() = lastLap;

        else
        {
            lapData.append(lastLap);
            posHistory.append(lastLap.pos);
        }

        releasedFromPits = false;

        if (lastLap < sessionRecords.bestLap)
            sessionRecords.bestLap = lastLap;

        //best sectors
//            if (colorData[LTPackets::RACE_SECTOR_1] == LTPackets::GREEN || colorData[LTPackets::RACE_SECTOR_1] == LTPackets::VIOLET)
//            {
//                bestSectors[0].first = lapData.last().sector1;
//                bestSectors[0].second = lapData.last().numLap;
//            }
//            if (colorData[LTPackets::RACE_SECTOR_2] == LTPackets::GREEN || colorData[LTPackets::RACE_SECTOR_2] == LTPackets::VIOLET)
//            {
//                bestSectors[1].first = lapData.last().sector2;
//                bestSectors[1].second = lapData.last().numLap;
//            }

        updateSectorRecords();

        if (ed.getFlagStatus() == LTPackets::SAFETY_CAR_DEPLOYED || ed.getFlagStatus() == LTPackets::RED_FLAG)
            lapData.last().raceLapExtraData.scLap = true;

        else
            lapData.last().raceLapExtraData.scLap = false;

    }

    //driver was set out from the pits, if he pits again on the next lap we will add it
    if (lastLap.lapTime.toString() == "OUT")
        releasedFromPits = true;
}

void DriverData::addFPQLap(const EventData &ed)
{
    //during practice and quali we only save timed laps
    if ((lastLap.lapTime.toString() != "") && (lapData.empty() ||
        (/*(lastLap.numLap > lapData.last().numLap) &&*/ lastLap.getSectorTime(1).toString() != "" && lastLap.getSectorTime(2).toString() != "" && lastLap.getSectorTime(3).toString() != "")))
    {
        bool correction = false;
        //sometimes servers messes up with lap numbers, we catch this if the numlap is <= than the last one
        if (!lapData.isEmpty() && lastLap.lapNum+1 <= lapData.last().lapNum)
        {
            correction = true;
            bool approx = lapData.last().qualiLapExtraData.approxLap || lapData.last().practiceLapExtraData.approxLap;
            int numlap = lapData.last().lapNum-1;
            lapData.last() = LapData(lastLap);
            lapData.last().qualiLapExtraData.approxLap = approx;
            lapData.last().practiceLapExtraData.approxLap = approx;

            if (lapData.size() > 1)
                lapData.last().lapNum = numlap;


            if (sessionRecords.bestLap.lapNum == numlap)
                sessionRecords.bestLap.lapNum = lapData.last().lapNum;
        }
        else
        {

            //if decryption fails, replace the garbage we obtained with the best lap time
            if (lastLap.lapTime.toString() != "" && !lastLap.lapTime.isValid())
                lastLap.lapTime = sessionRecords.bestLap.lapTime;

            lastLap.qualiLapExtraData.sessionTime = ed.getRemainingTime();
            lastLap.practiceLapExtraData.sessionTime = ed.getRemainingTime();
            lapData.append(lastLap);
            lapData.last().lapNum--;

            if (ed.getEventType() == LTPackets::QUALI_EVENT)
            {
                int qPeriod = ed.getQualiPeriod() > 0 ? ed.getQualiPeriod() : 1;
                lastLap.qualiLapExtraData.qualiPeriod = qPeriod;
                lapData.last().qualiLapExtraData.qualiPeriod = qPeriod;

            }

            updateSectorRecords();
        }

        if (!correction)
        {            
            if (ed.getEventType() == LTPackets::PRACTICE_EVENT)
            {            
                if (lastLap < sessionRecords.bestLap)
                    sessionRecords.bestLap = lapData.last();

                else if (lastLap.lapTime == sessionRecords.bestLap.lapTime)
                {
                    lapData.last().lapTime = LapData::sumSectors(lapData.last().getSectorTime(1).toString(), lapData.last().getSectorTime(2).toString(), lapData.last().getSectorTime(3).toString());
                    lapData.last().practiceLapExtraData.approxLap = true;
                }
                else
                    lapData.last().practiceLapExtraData.approxLap = false;

            }
            else if (ed.getEventType() == LTPackets::QUALI_EVENT)
            {
                if (lastLap < sessionRecords.bestLap)
                    sessionRecords.bestLap = lapData.last();

                if (lastLap < sessionRecords.bestQLaps[lastLap.qualiLapExtraData.qualiPeriod-1] ||
                        !sessionRecords.bestQLaps[lastLap.qualiLapExtraData.qualiPeriod-1].getTime().isValid())
                {
                    sessionRecords.bestQLaps[lastLap.qualiLapExtraData.qualiPeriod-1] = lapData.last();

                    if (sessionRecords.bestQLaps[lastLap.qualiLapExtraData.qualiPeriod-1] < sessionRecords.bestLap)
                        sessionRecords.bestLap = sessionRecords.bestQLaps[lastLap.qualiLapExtraData.qualiPeriod-1];
                }
                //if the current lap time is the same as the best lap, probably the driver hasn't improved so we have to calculate the real lap time from the sectors time
                else if (lastLap.lapTime == sessionRecords.bestQLaps[lastLap.qualiLapExtraData.qualiPeriod-1].lapTime)
                {
                    lapData.last().lapTime = LapData::sumSectors(lapData.last().getSectorTime(1).toString(), lapData.last().getSectorTime(2).toString(), lapData.last().getSectorTime(3).toString());
                    lapData.last().qualiLapExtraData.approxLap = true;
                }
                else
                    lapData.last().qualiLapExtraData.approxLap = false;

                correctPosition(ed);
            }

        }
        lapData.last().gap = QString::number((lapData.last().lapTime - ed.getSessionRecords().getFastestLap().getTime()).toDouble());

        posHistory.append(lastLap.pos);
        outLap = false;
    }

    //saving in and out laps
    else if (lastLap.lapNum > 1)
    {
        if (lapData.isEmpty() || (!lapData.isEmpty() && lapData.last().lapNum < lastLap.lapNum-1))
        {
            if (outLap == true || inPits == false)
            {
                lapData.append(lastLap);
                lapData.last().lapTime = LapTime("OUT LAP");

                if (inPits == true)
                    lapData.last().lapTime = LapTime("INST. LAP");

                lapData.last().lapNum--;
                outLap = false;
            }
            else
            {
                lapData.append(lastLap);
                lapData.last().lapTime = LapTime("IN LAP");
                lapData.last().lapNum--;
            }
            lapData.last().qualiLapExtraData.sessionTime = ed.getRemainingTime();
            lapData.last().practiceLapExtraData.sessionTime = ed.getRemainingTime();

            if (ed.getEventType() == LTPackets::QUALI_EVENT)
            {
                int qPeriod = ed.getQualiPeriod() > 0 ? ed.getQualiPeriod() : 1;
                lastLap.qualiLapExtraData.qualiPeriod = qPeriod;
                lapData.last().qualiLapExtraData.qualiPeriod = qPeriod;
            }

            updateSectorRecords();
        }
    }
}

void DriverData::updatePitStatus(LTPackets::Colors pitColor, EventData &ed)
{
    if (pitColor == LTPackets::PIT)
    {
        bool prev = inPits;
        inPits = true;
        if (prev == false && ed.getEventType() != LTPackets::RACE_EVENT)
            addInLap(ed);
    }


    else
    {
        //if driver went out from pit, he is on his outlap
        if (inPits == true)
            outLap = true;

        inPits = false;
    }
}

void DriverData::addInLap(const EventData &ed)
{
    if (lastLap.lapNum <= 0)
        return;

    lapData.append(lastLap);

    if (outLap == true)
        lapData.last().lapTime = LapTime("INST. LAP");
    else
        lapData.last().lapTime = LapTime("IN LAP");

    lapData.last().qualiLapExtraData.sessionTime = ed.getRemainingTime();
    lapData.last().practiceLapExtraData.sessionTime = ed.getRemainingTime();

    if (ed.getEventType() == LTPackets::QUALI_EVENT)
    {
        int qPeriod = ed.getQualiPeriod() > 0 ? ed.getQualiPeriod() : 1;
        lastLap.qualiLapExtraData.qualiPeriod = qPeriod;
        lapData.last().qualiLapExtraData.qualiPeriod = qPeriod;
    }
    updateSectorRecords();
}

void DriverData::updateLastLap()
{
    if (!lapData.isEmpty() && lapData.last().lapNum == lastLap.lapNum)
    {
        if (lapData.last().lapTime.toString() != "IN PIT" && lapData.last().getSectorTime(3).toString() == "" && lastLap.getSectorTime(3).toString() != "")
        {
            if (/*lapData.last().lapTime.toString() != "IN PIT" && lapData.last().sector1.toString() == "" &&*/ lastLap.getSectorTime(1).toString() != "")
                lapData.last().sectorTimes[0] = lastLap.sectorTimes[0];

            if (/*lapData.last().lapTime.toString() != "IN PIT" && lapData.last().sector2.toString() == "" && */lastLap.getSectorTime(2).toString() != "")
                lapData.last().sectorTimes[1] = lastLap.sectorTimes[1];

            lapData.last().sectorTimes[2] = lastLap.sectorTimes[2];

            updateSectorRecords();

            if (lapData.last().lapNum == sessionRecords.bestLap.lapNum)
            {
                sessionRecords.bestLap.sectorTimes[0] = lapData.last().sectorTimes[0];
                sessionRecords.bestLap.sectorTimes[1] = lapData.last().sectorTimes[1];
                sessionRecords.bestLap.sectorTimes[2] = lapData.last().sectorTimes[2];
            }

        }
    }
}

void DriverData::updateInPit()
{
    if (!lapData.isEmpty())
    {
        lapData.last().pos = lastLap.pos;
        lapData.last().gap = lastLap.gap;
        lapData.last().interval = lastLap.interval;
        posHistory.last() = lastLap.pos;
    }
}

//during quali and free practice - gap for all laps is calculated to the best session time
void DriverData::updateGaps(const EventData &ed)
{
    for (int i = 0; i < lapData.size(); ++i)
        lapData[i].gap = QString::number((lapData[i].lapTime - ed.getSessionRecords().getFastestLap().getTime()).toDouble());


}

void DriverData::updateSectorRecords()
{
    for (int i = 0; i < 3; ++i)
    {
        if ( sessionRecords.bestSectors[i].second == 0 ||
             (((lapData.last().getSectorTime(i + 1) < sessionRecords.bestSectors[i].first) ||
               ((lapData.last().getSectorTime(i + 1) == sessionRecords.bestSectors[i].first) &&
                (colorData.sectorColor(i + 1) == LTPackets::VIOLET || colorData.sectorColor(i + 1) == LTPackets::GREEN))) &&
              sessionRecords.bestSectors[i].second != 0))
        {
            sessionRecords.bestSectors[i] = QPair<LapTime, int>(LapTime(lapData.last().getSectorTime(i + 1)), lapData.last().lapNum);
        }
    }
}

int DriverData::lapDiff(LapTime *lap)
{
    int msec;

    if (!lap[0].isValid())
        msec = LapTime("59:59.999").toMsecs();
    else
        msec = lap[0].toMsecs();

    int idx = 0;

    for (int i = 1; i < 4; ++i)
    {
        if (lap[i].isValid() && lap[i].toMsecs() < msec)
        {
            idx = i;
            msec = lap[i].toMsecs();
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        if (i != idx && lap[i].isValid())
            lap[i] = lap[i] - lap[idx];
    }
    return idx;
}

void DriverData::addPitStop(const PitData &pd)
{
    if (pd.pitLap == 0)
        return;

    for (int i = 0; i < pitData.size(); ++i)
    {
        if (pitData[i].pitLap == pd.pitLap)
        {
            if (pitData[i].pitTime == "")
                pitData[i].pitTime = pd.pitTime;

            return;
        }
    }
    pitData.append(pd);
    qSort(pitData);
}
