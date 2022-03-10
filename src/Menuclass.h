
#ifndef ARDUINO_MENUCLASS_H
#define ARDUINO_MENUCLASS_H
#include <Arduino.h>

class Menuclass  
{
	private:
		int _maxIndex;
		int _index = 0;
	public:

		Menuclass(int indexSize);
		~Menuclass();
		void test();
		char Lineone[16];
		char Linetwo[16];
		void increaseindex();
		void decreaseindex();
		int indexPosition() {return _index;};
		void setindex(int setpoint);
		void clearline(int line);

};
#endif
