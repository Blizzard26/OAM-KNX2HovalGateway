#pragma once

#include "HovalMessage.h"
#include "HovalProtocolDecoder.h"

#define HOVAL_UNIT_ID 8
#define HOVAL_HOMEVENT (512 >> 4)
#define HOVAL_BM (1024 >> 4)
#define HOVAL_FUNCTION_GROUP_VENTILATION 50

/**
TypeName: S16
DatapointName: Temperatur Aussenluft
Decimal: 1 -> Divide by 10
FunctionGroup name: Lüftung
Min. value: -300
Max. value: 500
Writable: No
unit: °C
Commentary: Temperatur Aussenluft (Gemessene Lufttemperatur beim Eintritt der Aussenluft in das Gerät, kann von der effektiven Aussenlufttemp.  abweichen)
*/
const HovalMessageType TemperatureOutdoor(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 0, HovalDataType::S16, 1, DataType::FLOAT);

const HovalMessageType DisplayState(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 503, HovalDataType::U8, 0, DataType::UINT8);
const HovalMessageType ActiveWeek(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 504, HovalDataType::U8, 0, DataType::UINT8);
const HovalMessageType ActiveWeekName(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 505, HovalDataType::RAW, 0, DataType::STRING);

const HovalMessageType DeviceName(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 4005, HovalDataType::RAW, 0, DataType::STRING);

/**
TypeName: U16
DatapointName: Party Modus
Decimal: 1
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 240
Writable: Yes
unit: h
Commentary: PartyModus
*/
const HovalMessageType PartyMode(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 2010, HovalDataType::U16, 1, DataType::FLOAT, 0, 240);
// Originally Decimal: 1

/**
TypeName: U16
DatapointName: Pause Modus
Decimal: 1
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 240
Writable: Yes
unit: h
Commentary: PauseModus
*/
const HovalMessageType PauseMode(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 2018, HovalDataType::U16, 1, DataType::FLOAT, 0, 240);
// Originally Decimal: 1

/**
TypeName: U8
DatapointName: Feuchtigkeit Abluft
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 100
Writable: No
unit: %
Commentary: Feuchtigkeit Abluft (gemessene rel. Luftfeuchte)
*/
const HovalMessageType HumidityExhaust(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 37600, HovalDataType::U8, 0, DataType::UINT8);

/**
TypeName: S16
DatapointName: Temperatur Abluft
Decimal: 1 - Divide by 10
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 500
Writable: No
unit: °C
Commentary: Temperatur Abluft (Gemessene Lufttemperatur beim Eintritt der Abluft in das Gerät)
*/
const HovalMessageType TemperatureExhaust(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 37602, HovalDataType::S16, 1, DataType::FLOAT);

/**
TypeName: U8
DatapointName: VOC Abluft
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 100
Writable: No
unit: %
Commentary: VOC Abluft (gemessener VOC-Wert in der Abluft (CO2-Äquivalent)
*/
const HovalMessageType VocExhaust(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 37608, HovalDataType::U8, 0, DataType::UINT8);

/**
TypeName: U8
DatapointName: VOC Aussenluft
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 100
Writable: No
unit: %
Commentary: VOC Aussenluft (gemessener VOC-Wert in der Aussenluft (CO2-Äquivalent)
*/
const HovalMessageType VocOutdoor(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 37611, HovalDataType::U8, 0, DataType::UINT8);

/**
TypeName: U8
DatapointName: Ventilator Fortluft soll
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 100
Writable: No
unit: %
Commentary: Luftmenge Ist (ergibt sich aus "Luftmenge soll", "Feuchte soll", "Feuchte ist", "VOC Abluft", "VOC Aussenluft", "Temperatur Aussenluft")
*/
const HovalMessageType VentilationExhaust(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 38600, HovalDataType::U8, 0, DataType::UINT8);

/**
TypeName: U8
DatapointName: Lüftungsmodulation
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 15
Max. value: 100
Writable: No
unit: %
Commentary: Luftmenge Soll (Vorgebene Luftmenge, ergibt sich aus Betriebsart und Einstellung)
*/
const HovalMessageType Modulation(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 38606, HovalDataType::U8, 0, DataType::UINT8);

/**
TypeName: LIST
DatapointName: Luftqualität Regulierung
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 0x00000003
Max. value: 0
Writable: Yes
unit:
Commentary: Luftqualität Regulierung (Statusangabe ob Luftqualitätsregelung eingeschaltet ist)
values: 0=Gerät aus, z.B. Standbybetrieb; 1=Normaler Lüftungsbetrieb; 2=VOC Modus aktiv; 3=Feuchtigkeitsmodus aktiv; 4=Frostschutz aktiv; 5=CoolVet aktiv;
6=Fehlerzustand
*/
const HovalMessageType AirQualityControl(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 39600, HovalDataType::U8, 0, DataType::LIST, 0, 6);

/**
TypeName: U8
DatapointName: Status Lüftungsregelung
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 0
Max. value: 0
Writable: No
unit:
Commentary: Status Lüftungsregelung (Zahlencode der Auskunft über den Status des Lüftungsgerätes gibt gibt)
*/
const HovalMessageType VentilationControlStatus(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 39652, HovalDataType::U8, 0, DataType::LIST);

/**
    TypeName: LIST
    DatapointName: Betriebswahl Lüftung
    Decimal: 0
    FunctionGroup name: Lüftung
    Min. value: 0x00000037
    Max. value: 0
    Writable: Yes
    unit:
    Commentary: Betriebswahl Lüftung (z.B. Woche 1 / 2, Konstant, Standby)
    Values: 0=Standby, 1=Woche 1, 2=Woche 2, 4=Konstantbetrieb (Sollwert in 40651), 5=Sparbetrieb (Sollwert in 40686)
    */
const HovalMessageType OperatingMode(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 40650, HovalDataType::U8, 0, DataType::LIST, 0, 5);

/**
TypeName: U8
DatapointName: Normal-Lüftungsmodulation
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 15
Max. value: 100
Writable: Yes
unit: %
Commentary: Normal-Lüftungsmodulation (Luftmenge bei "Konstant"-Betrieb)
*/
const HovalMessageType NormalModulation(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 40651, HovalDataType::U8, 0, DataType::UINT8, 15, 100);

/**
TypeName: U8
DatapointName: Spar-Lüftungsmodulation
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 15
Max. value: 100
Writable: Yes
unit: %
Commentary: Spar-Lüftungsmodulation (Luftmenge bei Sparbetrieb)
*/
const HovalMessageType EconomyModulation(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 40686, HovalDataType::U8, 0, DataType::UINT8, 15, 100);

/**
TypeName: U8
DatapointName: Feuchte Sollwert
Decimal: 0
FunctionGroup name: Lüftung
Min. value: 30
Max. value: 65
Writable: Yes
unit: %
Commentary: Feuchte Sollwert (Am Bediengerät eingestellte maximale rel. Luftfeuchte)
*/
const HovalMessageType HumiditySetpoint(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 40687, HovalDataType::U8, 0, DataType::UINT8, 30, 65);

//??
const HovalMessageType PartyPauseValue(HOVAL_HOMEVENT, HOVAL_FUNCTION_GROUP_VENTILATION, 0, 40714, HovalDataType::U8, 0, DataType::UINT8, 15, 100);

const HovalMessageType VentilatorExhaustPWM(HOVAL_HOMEVENT, 0, 0, 41600, HovalDataType::U8, 0, DataType::UINT8);
const HovalMessageType VentilatorOutdoorPWM(HOVAL_HOMEVENT, 0, 0, 41601, HovalDataType::U8, 0, DataType::UINT8);
const HovalMessageType RPMRotor1(HOVAL_HOMEVENT, 0, 0, 41604, HovalDataType::U16, 2, DataType::FLOAT);
const HovalMessageType RPMRotor2(HOVAL_HOMEVENT, 0, 0, 41605, HovalDataType::U16, 2, DataType::FLOAT);

const HovalMessageType Welcome(HOVAL_HOMEVENT, 0, 0, 4005, HovalDataType::RAW, 0, DataType::STRING);

// To reset write (function code 0x46) 0x07  to MaintenanceResetCounter
const HovalMessageType MaintenanceResetCounter(HOVAL_HOMEVENT, 0, 0, 21054, HovalDataType::U8, 0, DataType::UINT8);
const HovalMessageType MaintenanceRemainingTime(HOVAL_HOMEVENT, 0, 0, 21058, HovalDataType::RAW, 0, DataType::RAW);
const HovalMessageType MaintenanceRemainingTime2(HOVAL_HOMEVENT, 0, 0, 20037, HovalDataType::RAW, 0, DataType::RAW);

// Hoval Error Messages
const HovalMessageType ActiveError1(HOVAL_HOMEVENT, 0, 0, 29042, HovalDataType::RAW, 0, DataType::ERRORTYPE);
const HovalMessageType ActiveError2(HOVAL_HOMEVENT, 0, 0, 29043, HovalDataType::RAW, 0, DataType::ERRORTYPE);
const HovalMessageType ActiveError3(HOVAL_HOMEVENT, 0, 0, 29044, HovalDataType::RAW, 0, DataType::ERRORTYPE);
const HovalMessageType ActiveError4(HOVAL_HOMEVENT, 0, 0, 29045, HovalDataType::RAW, 0, DataType::ERRORTYPE);
const HovalMessageType ActiveError5(HOVAL_HOMEVENT, 0, 0, 29046, HovalDataType::RAW, 0, DataType::ERRORTYPE);

// const HovalMessageType DevicePasswordLevel5(HOVAL_BM, 89, 1, 5, HovalDataType::RAW, 0, DataType::STRING);
// const HovalMessageType DevicePasswordLevel7(HOVAL_BM, 89, 1, 7, HovalDataType::RAW, 0, DataType::STRING);

// clang-format off
const HovalMessageType *HovalProtocolHandler::messageFilter[] = {
    &OperatingMode,
    &NormalModulation,
    &EconomyModulation,
    &Modulation,
    &HumiditySetpoint,
    &HumidityExhaust,
    &VocExhaust,
    &VocOutdoor,
    &AirQualityControl,
    &VentilationControlStatus,
    &TemperatureOutdoor,
    &TemperatureExhaust,
    &VentilationExhaust,
    &PartyMode,
    &PauseMode,
    &PartyPauseValue,
    // &Welcome,
    &MaintenanceRemainingTime,
    &MaintenanceRemainingTime2,
    &ActiveError1,
    &ActiveError2,
    &ActiveError3,
    &ActiveError4,
    &ActiveError5,
    &ActiveWeek,
    &ActiveWeekName,
    &DisplayState,
    &DeviceName,
    //&DevicePasswordLevel5,
    //&DevicePasswordLevel7,
    &VentilatorExhaustPWM,
    &VentilatorOutdoorPWM,
    &RPMRotor1,
    &RPMRotor2
  };
// clang-format on
const uint8_t HovalProtocolHandler::numberOfMessageFilters = sizeof(messageFilter) / sizeof(HovalMessageType*);
