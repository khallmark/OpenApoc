#pragma once

#include "library/strings.h"
#include <cstdint>

namespace OpenApoc
{

// ---------------------------------------------------------------------------
// The vanilla time base
//
// The original game's simulation runs at 36 ticks per second. That is not
// folklore -- there is no printable "36 ticks" string in either retail binary,
// but the recovered invasion-delay formula pins the rate three separate times:
//
//   UFO2P.EXE (non-4 / ISO), FUN_0006d384 @ VA 0x6D384, file offset 0xCFA28
//     delay = 0x2F7600
//           + rand[0, 0xB04] * 0x870
//           + rand[0, 0xE10] * 0x24     (all in vanilla ticks)
//
//     0x24     =      36  = one second, at 36 TPS
//     0x870    =    2160  = one minute, at 36 TPS
//     0x2F7600 = 3110400  = one day,    at 36 TPS
//
// Read at 36 TPS the formula is "one day, plus up to 2820 minutes, plus up to
// 3600 seconds" -- three round units from one expression. No other tick rate
// makes all three whole, so the base is fixed by the data rather than chosen.
//
// The static_asserts below are the lock: change VANILLA_TICKS_PER_SECOND and the
// build stops, pointing back at the binary constants it would contradict.
static constexpr unsigned VANILLA_TICKS_PER_SECOND = 36;
static constexpr unsigned VANILLA_TICKS_PER_MINUTE = VANILLA_TICKS_PER_SECOND * 60;
static constexpr unsigned VANILLA_TICKS_PER_HOUR = VANILLA_TICKS_PER_MINUTE * 60;
static constexpr unsigned VANILLA_TICKS_PER_DAY = VANILLA_TICKS_PER_HOUR * 24;

static_assert(VANILLA_TICKS_PER_SECOND == 0x24, "vanilla second must match UFO2P FUN_0006d384");
static_assert(VANILLA_TICKS_PER_MINUTE == 0x870, "vanilla minute must match UFO2P FUN_0006d384");
static_assert(VANILLA_TICKS_PER_DAY == 0x2F7600, "vanilla day must match UFO2P FUN_0006d384");

// TICKS_MULTIPLIER is OpenApoc's own simulation resolution. Nothing in the
// original constrains it -- it exists so we can step finer than 36 Hz.
//
// Because it is ours to choose, changing it must stay a one-line change. Two
// rules keep it that way, and both are worth honouring in new code:
//
//   1. Never write a tick count that was derived from TICKS_PER_SECOND as a
//      literal. Derive it. (A hardcoded 144 does not move when this does.)
//   2. Quantities recovered from the original are in VANILLA ticks. Store them
//      that way and convert once with vanillaTicks(), instead of baking the
//      multiplier into the stored value.
static constexpr unsigned TICKS_MULTIPLIER = 4;

static constexpr unsigned TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER;
static constexpr unsigned TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
static constexpr unsigned TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
static constexpr unsigned TICKS_PER_DAY = TICKS_PER_HOUR * 24;
static constexpr unsigned TURBO_TICKS = 5 * 60 * TICKS_PER_SECOND;

// Convert a quantity recovered from the original game into engine ticks.
constexpr uint64_t vanillaTicks(uint64_t vanilla) { return vanilla * TICKS_MULTIPLIER; }

// The engine and vanilla clocks must agree on what a second, a minute and a day
// are, whatever the multiplier is set to.
static_assert(TICKS_PER_SECOND == vanillaTicks(VANILLA_TICKS_PER_SECOND), "second must scale");
static_assert(TICKS_PER_MINUTE == vanillaTicks(VANILLA_TICKS_PER_MINUTE), "minute must scale");
static_assert(TICKS_PER_DAY == vanillaTicks(VANILLA_TICKS_PER_DAY), "day must scale");
// ---------------------------------------------------------------------------

class GameTime
{
  private:
	bool secondPassedFlag = false;
	bool fiveMinutesPassedFlag = false;
	bool hourPassedFlag = false;
	bool dayPassedFlag = false;
	bool weekPassedFlag = false;
	// Exact number of each boundary crossed by the last addTicks(). The booleans
	// above saturate at "one or more"; these do not.
	unsigned secondsPassedCounter = 0;
	unsigned fiveMinuteIntervalsPassedCounter = 0;
	unsigned hoursPassedCounter = 0;
	unsigned daysPassedCounter = 0;
	unsigned weeksPassedCounter = 0;

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

	// Exact crossing counts for the last addTicks(). A caller that advances more
	// than one interval in a single step must drive the matching handler this
	// many times; the bool accessors above cannot express that.
	unsigned secondsPassedCount() const { return secondsPassedCounter; }
	unsigned fiveMinuteIntervalsPassedCount() const { return fiveMinuteIntervalsPassedCounter; }
	unsigned hoursPassedCount() const { return hoursPassedCounter; }
	unsigned daysPassedCount() const { return daysPassedCounter; }
	unsigned weeksPassedCount() const { return weeksPassedCounter; }

	void clearFlags();

	static GameTime midday();
};
} // namespace OpenApoc
