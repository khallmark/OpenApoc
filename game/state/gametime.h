#pragma once

#include "library/strings.h"
#include <cstdint>

namespace OpenApoc
{

// UFO2P non-4 FUN_0006d384 @ VA 0x6D384 / file 0xCFA28 expresses its invasion
// delay in vanilla ticks as:
//
//   0x2F7600 + random(0xB04) * 0x870 + random(0xE10) * 0x24
//
// Under the community-observed 36-TPS interpretation, 0x24 is one second,
// 0x870 one minute, and 0x2F7600 one day. Their 1:60:86400 ratio corroborates
// that interpretation, but does not determine the absolute cadence by itself;
// for example, 18 TPS would make them two seconds, two minutes, and two days.
static constexpr unsigned VANILLA_TICKS_PER_SECOND = 36;
static constexpr unsigned VANILLA_TICKS_PER_MINUTE = VANILLA_TICKS_PER_SECOND * 60;
static constexpr unsigned VANILLA_TICKS_PER_HOUR = VANILLA_TICKS_PER_MINUTE * 60;
static constexpr unsigned VANILLA_TICKS_PER_DAY = VANILLA_TICKS_PER_HOUR * 24;

static_assert(VANILLA_TICKS_PER_SECOND == 0x24,
              "selected 36-TPS interpretation must match the invasion quantum");
static_assert(VANILLA_TICKS_PER_MINUTE == 0x870,
              "selected 36-TPS interpretation must match the invasion minute coefficient");
static_assert(VANILLA_TICKS_PER_DAY == 0x2F7600,
              "selected 36-TPS interpretation must match the invasion day coefficient");

// OpenApoc uses a finer simulation resolution than the original game.
static constexpr unsigned TICKS_MULTIPLIER = 4;
static constexpr unsigned TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER;
static constexpr unsigned TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
static constexpr unsigned TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
static constexpr unsigned TICKS_PER_DAY = TICKS_PER_HOUR * 24;
static constexpr unsigned TURBO_TICKS = 5 * 60 * TICKS_PER_SECOND;

constexpr uint64_t vanillaTicks(uint64_t ticks) { return ticks * TICKS_MULTIPLIER; }

static_assert(TICKS_PER_SECOND == vanillaTicks(VANILLA_TICKS_PER_SECOND),
              "engine second must scale from vanilla ticks");
static_assert(TICKS_PER_MINUTE == vanillaTicks(VANILLA_TICKS_PER_MINUTE),
              "engine minute must scale from vanilla ticks");
static_assert(TICKS_PER_DAY == vanillaTicks(VANILLA_TICKS_PER_DAY),
              "engine day must scale from vanilla ticks");

// UFO2P non-4 FUN_0005d1d8 @ VA 0x5D1D8 / file 0xBF87C is inclusive [0, n].
// FUN_000ad148 @ file 0xAD231 performs the initial invasion-delay write.
static constexpr int INVASION_DELAY_MINUTE_MAX = 0xB04;
static constexpr int INVASION_DELAY_SECOND_MAX = 0xE10;
static constexpr unsigned VANILLA_INVASION_DELAY_BASE = VANILLA_TICKS_PER_DAY;
static constexpr unsigned VANILLA_INVASION_DELAY_MINUTE = VANILLA_TICKS_PER_MINUTE;
static constexpr unsigned VANILLA_INVASION_DELAY_SECOND = VANILLA_TICKS_PER_SECOND;

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
