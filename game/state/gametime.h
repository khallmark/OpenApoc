#pragma once

#include "library/strings.h"
#include <cstdint>

namespace OpenApoc
{

static constexpr unsigned VANILLA_TICKS_PER_SECOND = 36;
static constexpr unsigned TICKS_MULTIPLIER = 4;
static constexpr unsigned TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER;
static constexpr unsigned TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
static constexpr unsigned TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
static constexpr unsigned TICKS_PER_DAY = TICKS_PER_HOUR * 24;
static constexpr unsigned TURBO_TICKS = 5 * 60 * TICKS_PER_SECOND;

// UFO2P non-4 FUN_0005d1d8 @ VA 0x5D1D8 / file 0xBF87C is inclusive [0, n].
// Invasion delay: FUN_0006d384 @ VA 0x6D384 / file 0xCFA28 and init write
// FUN_000ad148 @ file 0xAD231. Vanilla ticks:
//   0x2F7600 + FUN_0005d1d8(0xB04) * 0x870 + FUN_0005d1d8(0xE10) * 0x24
static constexpr int INVASION_DELAY_MINUTE_MAX = 0xB04;
static constexpr int INVASION_DELAY_SECOND_MAX = 0xE10;
static constexpr unsigned VANILLA_INVASION_DELAY_BASE = 0x2F7600;
static constexpr unsigned VANILLA_INVASION_DELAY_MINUTE = 0x870;
static constexpr unsigned VANILLA_INVASION_DELAY_SECOND = 0x24;

inline uint64_t vanillaInvasionDelayTicks(int minuteRoll, int secondRoll)
{
	return static_cast<uint64_t>(TICKS_PER_DAY) +
	       static_cast<uint64_t>(minuteRoll) * TICKS_PER_MINUTE +
	       static_cast<uint64_t>(secondRoll) * TICKS_PER_SECOND;
}

class GameTime
{
  private:
	bool secondPassedFlag = false;
	bool fiveMinutesPassedFlag = false;
	bool hourPassedFlag = false;
	bool dayPassedFlag = false;
	bool weekPassedFlag = false;

  public:
	uint64_t ticks = 0;
	GameTime() = default;
	GameTime(uint64_t ticks);

	void addTicks(uint64_t ticks);

	unsigned int getHours() const;

	unsigned int getMinutes() const;

	unsigned int getSeconds() const;

	unsigned int getMonthDay() const;

	unsigned int getDay() const;

	unsigned int getWeek() const;

	unsigned int getMonth() const;

	unsigned int getFirstDayOfCurrentWeek() const;

	unsigned int getLastDayOfCurrentWeek() const;

	unsigned int getLastDayOfCurrentMonth() const;

	unsigned int getTicksBetween(unsigned int fromDays, unsigned int fromHours,
	                             unsigned int fromMinutes, unsigned int fromSeconds,
	                             unsigned int toDays, unsigned int toHours, unsigned int toMinutes,
	                             unsigned int toSeconds) const;
	uint64_t getTicks() const;

	// returns week with prefix
	UString getWeekString() const;

	// returns formatted time in format hh:mm
	UString getShortTimeString() const;

	// returns formatted time in format hh:mm:ss
	UString getLongTimeString() const;

	// returns formatted date in format a, d m, y
	UString getLongDateString() const;

	// returns formatted date in format d m, y
	UString getShortDateString() const;

	// set at end of each second
	bool secondPassed() const;
	void setSecondPassed(bool newValue) { secondPassedFlag = newValue; }

	// set at end of each 5 minutes
	bool fiveMinutesPassed() const;
	void setFiveMinutesPassed(bool newValue) { fiveMinutesPassedFlag = newValue; }

	// set at end of each hour
	bool hourPassed() const;
	void setHourPassed(bool newValue) { hourPassedFlag = newValue; }

	// set at midnight
	bool dayPassed() const;
	void setDayPassed(bool newValue) { dayPassedFlag = newValue; }

	// set at sunday midnight
	bool weekPassed() const;
	void setWeekPassed(bool newValue) { weekPassedFlag = newValue; }

	void clearFlags();

	static GameTime midday();
};

// UFO2P city Speed1 advances the city clock every other rendered frame.
// OpenApoc cityview.cpp: skipSpeed1Tick starts false, so the first Speed1 frame is skipped.
inline unsigned vanillaCitySpeed1Ticks(unsigned proposedTicks, bool &skipThisFrame)
{
	skipThisFrame = !skipThisFrame;
	return skipThisFrame ? 0u : proposedTicks;
}

} // namespace OpenApoc
