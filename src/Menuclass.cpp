#include "Menuclass.h"  

Menuclass::Menuclass(int indexSize)
{
    _maxIndex = indexSize;
}

Menuclass::~Menuclass()
{

}

void Menuclass::test()
{
    Serial.println("Class test");
}


void Menuclass::increaseindex()
{
    //increase the index, if at max/min roll over
    _index++;
    if (_index > _maxIndex)
    {
        _index = 0;
    }
}

void Menuclass::decreaseindex()
{
    //increase the index, if at max/min roll over
    _index--;
    if (_index < 0)
    {
        _index = _maxIndex;
    }
}

void Menuclass::setindex(int setpoint){

    if (setpoint <= _maxIndex && setpoint >= 0)
    {
        _index = setpoint;
    }
}


void Menuclass::clearline(int line){
    char _clear[17] = "                ";
    switch (line)
    {
    case 1:
        strcpy(Lineone, _clear);
        break;
    
    case 2:
        strcpy(Linetwo, _clear);
        break;

    default:
        break;
    }
}