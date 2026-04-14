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

KNXValue listToDptState(HovalMessage* message, uint8_t value)
{
  return KNXValue(message->list() == value);
}

KNXValue listToDptStatus_Mode3(HovalMessage* message)
{
  return KNXValue(message->list());
}

KNXValue byteToDptStatus_Mode3(HovalMessage* message)
{
  return KNXValue(message->u8Value());
}

KNXValue hoursToDptTimePeriodMin(HovalMessage* message)
{
  // message contains hours as decimal value (e.g., 2.5h)
  uint16_t value = (uint16_t)(message->floatValue() * 60);
  return KNXValue(value);
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
  char* value = errorStringBuf;
  memset(value, 0, 15);

  ErrorMessage errorMessage = message->errorMessage();
  if (errorMessage.error_type != 0)
  {
    // TODO: uncomment after properly implementing HovalMessage::mapErrorType
    // snprintf(value, 15, "%c:%d", errorMessage.error_type, errorMessage.error_code);
    snprintf(value, 15, "%d:%d", errorMessage.error_type, errorMessage.error_code);
  }
  return KNXValue(value);
}

static uint8_t errorBitSet = 0;
KNXValue hasError(HovalMessage* message)
{
  uint8_t index = message->messageType->dataPointId - ActiveError1.dataPointId;
  if (message->errorMessage().error_type != 0)
  {
    errorBitSet |= 1 << index;
  }
  else
  {
    errorBitSet &= 0xFF ^ (1 << index);
  }
  return KNXValue(errorBitSet != 0);
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
#define DPT_Status3 Dpt(6, 20, 5)

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
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_week1), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 1); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_week2), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 2); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_constant), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 4); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &OperatingMode, homeVentComObject(HOV_Kovent_operating_mode_economy), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 5); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(&OperatingMode, homeVentComObject(HOV_Kovent_operating_mode), DPT_Status3, &listToDptStatus_Mode3, &sendOnChange,
                            &defaultSendIntervalMs),

    // 0=Gerät aus, z.B. Standbybetrieb; 1=Normaler Lüftungsbetrieb; 2=VOC Modus aktiv; 3=Feuchtigkeitsmodus aktiv; 4=Frostschutz aktiv; 5=CoolVet aktiv;
    // 6=Fehlerzustand
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_standby), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 0); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_normal), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 1); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_voc), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 2); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_humidity), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 3); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_antifreeze), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 4); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_coolvent), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 5); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(
        &AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol_error), DPT_State,
        [](HovalMessage* message) -> KNXValue { return listToDptState(message, 6); }, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(&AirQualityControl, homeVentComObject(HOV_Kovent_airqualitycontrol), DPT_Status3, &listToDptStatus_Mode3, &sendOnChange,
                            &defaultSendIntervalMs),

    HovalMessageTransformer(&VentilationControlStatus, homeVentComObject(HOV_Kovent_ventilation_control_status), DPT_Status3, &byteToDptStatus_Mode3,
                            &sendOnChange, &defaultSendIntervalMs), //

    HovalMessageTransformer(&Modulation, homeVentComObject(HOV_Kovent_modulation), DPT_Scaling, &percentToDptScaling, &sendOnChange, &defaultSendIntervalMs),
    HovalMessageTransformer(&NormalModulation, homeVentComObject(HOV_Kovent_normal_modulation), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&EconomyModulation, homeVentComObject(HOV_Kovent_economy_modulation), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs), //

    HovalMessageTransformer(&HumiditySetpoint, homeVentComObject(HOV_Kovent_humidity_setpoint), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs),
    HovalMessageTransformer(&HumidityExhaust, homeVentComObject(HOV_Kovent_humidity_exhaust), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs),
    HovalMessageTransformer(&VentilationExhaust, homeVentComObject(HOV_Kovent_ventilation_exhaust), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs), //

    // Sould be DPT_TimePeriodMin, but the implementation for sending it is flawed
    HovalMessageTransformer(&PartyMode, homeVentComObject(HOV_Kovent_party_mode), DPT_Value_2_Ucount, &hoursToDptTimePeriodMin, &sendOnChange,
                            &defaultSendIntervalMs), //
    HovalMessageTransformer(&PauseMode, homeVentComObject(HOV_Kovent_pause_mode), DPT_Value_2_Ucount, &hoursToDptTimePeriodMin, &sendOnChange,
                            &defaultSendIntervalMs),
    HovalMessageTransformer(&PartyPauseValue, homeVentComObject(HOV_Kovent_party_pause_value), DPT_Scaling, &percentToDptScaling, &sendOnChange,
                            &defaultSendIntervalMs), //

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
    // Be carefull here: Do not use DPT_String_ASCII as it prints a (useless) warning. Which causes the system to hang during bootup.
    // See dpt.cpp Line 11f
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

    // ActiveWeek
    // DeviceName
    // Welcome
};
const uint8_t Hoval2KNXMapper::numberOfMessageTransformers = sizeof(messageTransformers) / sizeof(HovalMessageTransformer);
