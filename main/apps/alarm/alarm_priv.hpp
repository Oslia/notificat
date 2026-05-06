#ifndef ALARM_PRIV_HPP_
#define ALARM_PRIV_HPP_

#include "alarm_model.hpp"
#include "alarm_view.hpp"

namespace Alarm {
	class AlarmPriv {
	public:
		AlarmPriv();
		
		void Update(AlarmState& model, const AppMsg& msg);

		AlarmState model;
		std::unique_ptr<MainView> main_view;
		std::unique_ptr<AlarmEditView> alarm_edit_view;
	};
}

#endif	// ALARM_PRIV_HPP_
