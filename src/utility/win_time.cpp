#include "win_time.hpp"


namespace bias
{
    GetTime::GetTime() {}

	GetTime::GetTime(long long unsigned int sec,long long unsigned int usec)
	{
            secs = secs;
            usec = usec;
	}


	/*int GetTime::getdaytime(struct timeval *tv, struct timezone *tz)
	{

            FILETIME ft;

            // Initialize the present time to 0 and the timezone to UTC
            unsigned __int64 tmpres = 0;
            static int tzflag = 0;

            if (NULL != tv)
            {
			
                GetSystemTimeAsFileTime(&ft);

                // The GetSystemTimeAsFileTime returns the number of 100 nanosecond 
                // intervals since Jan 1, 1601 in a structure. Copy the high bits to 
                // the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
                tmpres |= ft.dwHighDateTime;
                tmpres <<= 32;
                tmpres |= ft.dwLowDateTime;

                // Convert to microseconds by dividing by 10
                tmpres /= 10;

                // The Unix epoch starts on Jan 1 1970.  Need to subtract the difference 
                // in seconds from Jan 1 1601.
                tmpres -= DELTA_EPOCH_IN_MICROSECS;

                // Finally change microseconds to seconds and place in the seconds value. 
                // The modulus picks up the microseconds.
                tv->tv_sec = (long)(tmpres / 1000000UL);
                tv->tv_usec = (long)(tmpres % 1000000UL);
            }

            if (NULL != tz)
            {
                if (!tzflag)
                {
                    _tzset();
                    tzflag++;
                }

                // Adjust for the timezone west of Greenwich
                tz->tz_minuteswest = _timezone / 60;
                tz->tz_dsttime =_daylight;
            }

            return 0;

	}*/


	uint64_t GetTime::getPCtime()
	{

            //GetTime* getTime = new GetTime(0,0);
            //get computer local time since midnight
            /*getTime->curr_time = time(NULL);
            tm *tm_local = localtime(&getTime->curr_time);
            getTime->getdaytime(&getTime->tv, NULL);
            getTime->secs = (tm_local->tm_hour*3600) + tm_local->tm_min*60 + tm_local->tm_sec;
            getTime->usec = (unsigned int)getTime->tv.tv_usec;
            TimeStamp ts = {getTime->secs, unsigned int(getTime->usec)};*/

            // returns a time_point object 
            // https://www.cplusplus.com/reference/chrono/time_point/ 
            //https://stackoverflow.com/questions/46740302/where-does-the-time-from-stdchronosystem-clocknow-time-since-epoch-c   
            auto usec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());     

            return static_cast<uint64_t>(usec.count());
	}

}
