#include <memory>

#include "lvgl.h"
#include "app.hpp"

#include "alarm_model.hpp"

#define ALARM_ADD_BTN_WIDTH		75
#define ALARM_ADD_BTN_HEIGHT	75

namespace Alarm {
	class ClockComponent {
	public:
		ClockComponent(lv_obj_t* parent);
		~ClockComponent();
		void SetTime(const struct tm& tm);


	private:
		lv_obj_t* container;
		lv_obj_t* clock;
		uint8_t hour;
		uint8_t min;
	};


	class AlarmListComponent {
	public:
		AlarmListComponent(lv_obj_t* parent);
		~AlarmListComponent();
		static void EventHandler(lv_event_t* e);
		void DrawList(const AlarmState& model);

	private:
		void DrawListButton(lv_obj_t* button, const struct AlarmData& data);
		void CreateIndicator(lv_obj_t* button, uint8_t day, bool active);
		lv_obj_t* container;
		lv_obj_t* list;
		lv_obj_t* add_btn;
		struct AlarmData alarm_data[ALARM_MAX_NUM];
		int data_num;
	};


	class AlarmEditComponent {
	public:
		AlarmEditComponent(lv_obj_t* parent);
		~AlarmEditComponent();
		static void EventHandler(lv_event_t* e);
		void Draw(const AlarmState& model);


	private:
		lv_obj_t* container;
		lv_obj_t* hour_roller;
		lv_obj_t* min_roller;
		lv_obj_t* day_checkbox[7];
		lv_obj_t* save_btn;
		struct AlarmData alarm_data;
	};


	class MainView: public View<AlarmState> {
	public:
		MainView();

		~MainView();

		void Draw(const AlarmState& model) override;


	private:
		std::unique_ptr<ClockComponent> clock;
		std::unique_ptr<AlarmListComponent> list;
		lv_obj_t* tile_view;
	};

	
	class AlarmEditView: public View<AlarmState> {
	public:
		AlarmEditView();

		~AlarmEditView();

		void Draw(const AlarmState& model) override;


	private:
		std::unique_ptr<AlarmEditComponent> edit_component;
	};
}
