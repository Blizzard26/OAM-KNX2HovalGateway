#pragma once

////////////////////////
// Decoder configuration
////////////////////////

#include "Hoval2KNXMapper.h"
#include "HovalMessages.h"
#include "knxprod.h"
#include <knx/dpt.h>

KNXValue percentToDptScaling(HovalMessage* message)
{
  return KNXValue(message->u8Value());
}

HovalValue scalingToPercent(KNXValue& value)
{
  return HovalValue(value, DataType::UINT8);
}

KNXValue listToDptState(HovalMessage* message, uint8_t value)
{
  return KNXValue(message->list() == value);
}

HovalValue enumValueToList(uint8_t value)
{
  return HovalValue(value, DataType::LIST);
}

KNXValue toDptStatusMode3(uint8_t status, uint8_t mode)
{
  return KNXValue((uint8_t)((status & 0x1F) << 3 | (mode & 0x07)));
}

// DPT_Status3
KNXValue listToDptStatus_Mode3(HovalMessage* message)
{
  return toDptStatusMode3(message->list(), 0b001);
}

// DPT_Status3
KNXValue byteToDptStatus_Mode3(HovalMessage* message)
{
  return toDptStatusMode3(message->u8Value(), 0b001);
}

KNXValue hoursToDptTimePeriodMin(HovalMessage* message)
{
  // message contains hours as decimal value (e.g., 2.5h)
  uint16_t value = (uint16_t)(message->floatValue() * 60);
  return KNXValue(value);
}

HovalValue timePeriodMinToHours(KNXValue& value)
{
  return HovalValue((float)value / 60.f);
}

KNXValue tempToDeptValueTemp(HovalMessage* message)
{
  return KNXValue(message->floatValue());
}

KNXValue rpmToDptAngularFrequency(HovalMessage* message)
{
  // Reasonable DPT
  return KNXValue(message->floatValue() * 0.10472);
}

KNXValue maintenanceWeeksToHrs(HovalMessage* message)
{
  if (message->messageBodyLength < 4)
    return KNXValue((time_t)0);
  uint8_t weeks = message->messageBody[3];
  time_t hrs = (weeks * 24l * 7l);
  return KNXValue(hrs);
}

static char errorStringBuf[15];
KNXValue errorToString(HovalMessage* message)
{
  // We need to use a static field here, otherwise we either have issues with the local
  // variable being invalidate before the returned value is used or we create a memory leak.
  // Note that processing of the Group Value update copies the value into a new buffer.
  // Moreover, this method is used single threaded, therefore we're safe.
  memset(errorStringBuf, 0, sizeof(errorStringBuf));

  ErrorMessage errorMessage = message->errorMessage();
  if (errorMessage.error_type != 0)
  {
    snprintf(errorStringBuf, sizeof(errorStringBuf), "%c:%d", errorMessage.error_type, errorMessage.error_code);
    // snprintf(errorStringBuf, 15, "%d:%d", errorMessage.error_type, errorMessage.error_code);
  }
  return KNXValue(errorStringBuf);
}

static uint8_t errorBitSet = 0;
KNXValue hasError(HovalMessage* message)
{
  uint8_t index = message->messageType->dataPointId - ActiveError1.dataPointId;
  ErrorMessage em = message->errorMessage();
  if (em.error_type != 0)
  {
    errorBitSet |= 1u << index;
  }
  else
  {
    errorBitSet &= 0xFF ^ (1u << index);
  }
  return KNXValue(errorBitSet != 0);
}

static uint8_t errorBitField = 0;
// DPT_Status3
KNXValue activeErrors(HovalMessage* message)
{
  uint8_t index = message->messageType->dataPointId - ActiveError1.dataPointId;
  ErrorMessage em = message->errorMessage();
  if (em.error_type != '\0' && em.error_type != 'W')
  {
    errorBitField |= 1u << index;
  }
  else
  {
    errorBitField &= 0xFF ^ (1u << index);
  }
  return toDptStatusMode3(errorBitField, 0b100);
}

static uint8_t warningBitField = 0;
// DPT_Status3
KNXValue activeWarnings(HovalMessage* message)
{
  uint8_t index = message->messageType->dataPointId - ActiveError1.dataPointId;
  ErrorMessage em = message->errorMessage();
  if (em.error_type == 'W')
  {
    warningBitField |= 1u << index;
  }
  else
  {
    warningBitField &= 0xFF ^ (1u << index);
  }
  return toDptStatusMode3(warningBitField, 0b010);
}

KNXValue errorToAppearanceTimestamp(HovalMessage* message)
{
  ErrorMessage errorMessage = message->errorMessage();
  time_t appearance_time = errorMessage.appearance_time;
  tm time;
  gmtime_r(&appearance_time, &time);
  return KNXValue(time);
}

#ifdef HOV_KoOffset
#define homeVentComObject(ko) (ko + HOV_KoOffset)
#else
#define homeVentComObject(ko) ko
#endif
// Should actually be DPT_Status_Mode3, but sending that is strange / doesn't work
#define DPT_Status3 DPT_Value_1_Count

uint32_t defaultSendIntervalMs(HovalMessage* message)
{
  return ParamHOV_HVSendIntervalDelayTimeMS;
}

uint32_t errorSendIntervalMs(HovalMessage* message)
{
  ErrorMessage error = message->errorMessage();
  if (error.error_type != '\0')
  {
    return ParamHOV_HVSendIntervalDelayTimeMS;
  }
  else
  {
    // Do not repeat non-error messages
    return 0;
  }
}

bool sendOnChange()
{
  return ParamHOV_HVSendEveryChange;
}

// Hints for DPT https://www.promotic.eu/en/pmdoc/Subsystems/Comm/PmDrivers/KNXDTypes.htm
HovalMessageTransformer Hoval2KNXMapper::messageTransformers[] = {
    // 0=Standby, 1=Woche 1, 2=Woche 2, 4=Konstantbetrieb, Sollwert 40651, 5=Sparbetrieb, Sollwert 40686
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_standby), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 0); }, [](KNXValue& value) -> HovalValue { return enumValueToList(0); },
        &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_week1), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 1); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 1 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_week2), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 2); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 2 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_constant), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 4); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 4 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_economy), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 5); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 5 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode), DPT_Status3, &listToDptStatus_Mode3,
        [](KNXValue& value) -> HovalValue { return enumValueToList((((uint8_t)value >> 3) > 5) ? 0 : (uint8_t)value); }, &sendOnChange, &defaultSendIntervalMs),

    // 0=Gerät aus, z.B. Standbybetrieb; 1=Normaler Lüftungsbetrieb; 2=VOC Modus aktiv; 3=Feuchtigkeitsmodus aktiv; 4=Frostschutz aktiv;
    // 5=CoolVet aktiv;=Fehlerzustand
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_standby), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 0); }, [](KNXValue& value) -> HovalValue { return enumValueToList(0); },
        &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_normal), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 1); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 1 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_voc), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 2); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 2 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_humidity), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 3); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 3 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_antifreeze), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 4); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 4 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_coolvent), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 5); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 5 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_error), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 6); },
        [](KNXValue& value) -> HovalValue { return enumValueToList((bool)value ? 6 : 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol), DPT_Status3, &listToDptStatus_Mode3,
        [](KNXValue& value) -> HovalValue { return enumValueToList(((uint8_t)value > 6) ? 0 : (uint8_t)value); }, &sendOnChange, &defaultSendIntervalMs),

    HovalMessageTransformer(&VentilationControlStatus, homeVentComObject(HOV_Kovent_ventilation_control_status), DPT_Status3, &byteToDptStatus_Mode3,
                            &sendOnChange, &defaultSendIntervalMs), //

    HovalMessageTransformer(&Modulation, homeVentComObject(HOV_Kovent_modulation), DPT_Scaling, &percentToDptScaling, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(&NormalModulation, homeVentComObject(HOV_Kovent_normal_modulation), DPT_Scaling, &percentToDptScaling, &scalingToPercent,
                            &sendOnChange, &defaultSendIntervalMs), //
    HovalMessageTransformer(&EconomyModulation, homeVentComObject(HOV_Kovent_economy_modulation), DPT_Scaling, &percentToDptScaling, &scalingToPercent,
                            &sendOnChange, &defaultSendIntervalMs), //

    HovalMessageTransformer(&HumiditySetpoint, homeVentComObject(HOV_Kovent_humidity_setpoint), DPT_Scaling, &percentToDptScaling, &scalingToPercent,
                            &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(&HumidityExhaust, homeVentComObject(HOV_Kovent_humidity_exhaust), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs),
    HovalMessageTransformer(&VentilationExhaust, homeVentComObject(HOV_Kovent_ventilation_exhaust), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs), //

    // Sould be DPT_TimePeriodMin, but the implementation for sending it is flawed
    HovalMessageTransformer(&PartyMode, homeVentComObject(HOV_Kovent_party_mode), DPT_Value_2_Ucount, &hoursToDptTimePeriodMin, &timePeriodMinToHours,
                            &sendOnChange, &defaultSendIntervalMs, nullptr, homeVentComObject(HOV_Kovent_party_value)), //
    HovalMessageTransformer(&PauseMode, homeVentComObject(HOV_Kovent_pause_mode), DPT_Value_2_Ucount, &hoursToDptTimePeriodMin, &timePeriodMinToHours,
                            &sendOnChange, &defaultSendIntervalMs, nullptr, homeVentComObject(HOV_Kovent_pause_value)), //
    // party_value/pause_value share one physical Hoval register. They must never be updated from Hoval (that would
    // let one overwrite the other via echo) and must never be sent to Hoval on their own - only as a prerequisite
    // of PartyMode/PauseMode. Hence: never poll (activeFunc false), ignore incoming, send only as prerequisite.
    HovalMessageTransformer(
        &PartyPauseValue, homeVentComObject(HOV_Kovent_party_value), DPT_Scaling, &percentToDptScaling, &scalingToPercent,
        []() -> bool { return false; }, [](HovalMessage* message) -> uint32_t { return 0; }, []() -> bool { return false; }, -1, true, true), //
    HovalMessageTransformer(
        &PartyPauseValue, homeVentComObject(HOV_Kovent_pause_value), DPT_Scaling, &percentToDptScaling, &scalingToPercent,
        []() -> bool { return false; }, [](HovalMessage* message) -> uint32_t { return 0; }, []() -> bool { return false; }, -1, true, true), //

    HovalMessageTransformer(&TemperatureOutdoor, homeVentComObject(HOV_Kovent_temperature_outdoor), DPT_Value_Temp, &tempToDeptValueTemp, &sendOnChange,
                            &defaultSendIntervalMs),
    HovalMessageTransformer(&TemperatureExhaust, homeVentComObject(HOV_Kovent_temperature_exhaust), DPT_Value_Temp, &tempToDeptValueTemp, &sendOnChange,
                            &defaultSendIntervalMs), //

    HovalMessageTransformer(&VocExhaust, homeVentComObject(HOV_Kovent_voc_exhaust), DPT_Scaling, &percentToDptScaling, &sendOnChange, &defaultSendIntervalMs,
                            []() -> bool { return ParamHOV_HVVocSensorPresent; }), //
    HovalMessageTransformer(&VocOutdoor, homeVentComObject(HOV_Kovent_voc_outdoor), DPT_Scaling, &percentToDptScaling, &sendOnChange, &defaultSendIntervalMs,
                            []() -> bool { return ParamHOV_HVVocSensorPresent; }), //

    HovalMessageTransformer(&VentilatorExhaustPWM, homeVentComObject(HOV_Kogeneral_ventilator_exhaust_pwm), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&VentilatorOutdoorPWM, homeVentComObject(HOV_Kogeneral_ventilator_outdoor_pwm), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs), //

    HovalMessageTransformer(&RPMRotor1, homeVentComObject(HOV_Kogeneral_rpm_rotor1), DPT_Value_Angular_Frequency, &rpmToDptAngularFrequency, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&RPMRotor2, homeVentComObject(HOV_Kogeneral_rpm_rotor2), DPT_Value_Angular_Frequency, &rpmToDptAngularFrequency, &sendOnChange,
                            &defaultSendIntervalMs, []() -> bool { return ParamHOV_GeneralRpm2Present; }), //

    // Should be DPT_TimePeriodHrs, but the implementation for sending it is flawed
    HovalMessageTransformer(&MaintenanceRemainingTime, homeVentComObject(HOV_Kogeneral_maintenance_remaining), DPT_Value_2_Ucount, &maintenanceWeeksToHrs,
                            &sendOnChange, &defaultSendIntervalMs),

    HovalMessageTransformer(&ActiveError1, homeVentComObject(HOV_Kogeneral_active_error), DPT_Alarm, &hasError, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError2, homeVentComObject(HOV_Kogeneral_active_error), DPT_Alarm, &hasError, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError3, homeVentComObject(HOV_Kogeneral_active_error), DPT_Alarm, &hasError, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError4, homeVentComObject(HOV_Kogeneral_active_error), DPT_Alarm, &hasError, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError5, homeVentComObject(HOV_Kogeneral_active_error), DPT_Alarm, &hasError, &sendOnChange,
                            &defaultSendIntervalMs), //

    // Be carefull here: Do not use DPT_String_ASCII as it prints a (useless) warning. Which causes the system to hang during bootup. See dpt.cpp Line 11f
    HovalMessageTransformer(&ActiveError1, homeVentComObject(HOV_Kogeneral_active_error_1), DPT_String_8859_1, &errorToString, &sendOnChange,
                            &errorSendIntervalMs), //
    HovalMessageTransformer(&ActiveError2, homeVentComObject(HOV_Kogeneral_active_error_2), DPT_String_8859_1, &errorToString, &sendOnChange,
                            &errorSendIntervalMs), //
    HovalMessageTransformer(&ActiveError3, homeVentComObject(HOV_Kogeneral_active_error_3), DPT_String_8859_1, &errorToString, &sendOnChange,
                            &errorSendIntervalMs), //
    HovalMessageTransformer(&ActiveError4, homeVentComObject(HOV_Kogeneral_active_error_4), DPT_String_8859_1, &errorToString, &sendOnChange,
                            &errorSendIntervalMs), //
    HovalMessageTransformer(&ActiveError5, homeVentComObject(HOV_Kogeneral_active_error_5), DPT_String_8859_1, &errorToString, &sendOnChange,
                            &errorSendIntervalMs), //

    HovalMessageTransformer(&ActiveError1, homeVentComObject(HOV_Kogeneral_active_error_error), DPT_Status3, &activeErrors, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError2, homeVentComObject(HOV_Kogeneral_active_error_error), DPT_Status3, &activeErrors, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError3, homeVentComObject(HOV_Kogeneral_active_error_error), DPT_Status3, &activeErrors, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError4, homeVentComObject(HOV_Kogeneral_active_error_error), DPT_Status3, &activeErrors, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError5, homeVentComObject(HOV_Kogeneral_active_error_error), DPT_Status3, &activeErrors, &sendOnChange,
                            &defaultSendIntervalMs), //

    HovalMessageTransformer(&ActiveError1, homeVentComObject(HOV_Kogeneral_active_error_warning), DPT_Status3, &activeWarnings, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError2, homeVentComObject(HOV_Kogeneral_active_error_warning), DPT_Status3, &activeWarnings, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError3, homeVentComObject(HOV_Kogeneral_active_error_warning), DPT_Status3, &activeWarnings, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError4, homeVentComObject(HOV_Kogeneral_active_error_warning), DPT_Status3, &activeWarnings, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&ActiveError5, homeVentComObject(HOV_Kogeneral_active_error_warning), DPT_Status3, &activeWarnings, &sendOnChange,
                            &defaultSendIntervalMs), //

    // ActiveWeek
    // DeviceName
    // Welcome
};
const uint8_t Hoval2KNXMapper::numberOfMessageTransformers = sizeof(messageTransformers) / sizeof(HovalMessageTransformer);
