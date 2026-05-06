#ifndef ALARM_MODEL_HPP_
#define ALARM_MODEL_HPP_

#include <time.h>

namespace Alarm {
	#define ALARM_MAX_NUM	16

	enum class AlarmViewId {
		VIEW_MAIN,
		VIEW_ALARM_LIST,
		VIEW_ALARM_SET,
	};

	struct AlarmData {
		uint8_t hour;
		uint8_t min;
		uint8_t days;
		bool repeat;
		bool activated;
	};
	
	struct AlarmState {
	public:
		time_t now;
        struct tm timeinfo;
		AlarmViewId view_id;
		struct AlarmData alarm_data[ALARM_MAX_NUM];
		int alarm_num;
		int editing_index;
		AlarmData editing_alarm;
	};
}


#endif // ALARM_MODEL_HPP_
