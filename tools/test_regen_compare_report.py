#!/usr/bin/env python3
"""Pure regression tests for deterministic compare-report generation."""

from collections import Counter

from regen_compare_report import ranked_counts


def test_ranked_counts_breaks_ties_by_name() -> None:
    first = Counter()
    first.update({"zeta": 3, "alpha": 3, "middle": 1})
    second = Counter()
    second.update({"middle": 1, "alpha": 3, "zeta": 3})

    expected = [("alpha", 3), ("zeta", 3), ("middle", 1)]
    assert ranked_counts(first, 3) == expected
    assert ranked_counts(second, 3) == expected
    assert ranked_counts(first, 2) == expected[:2]


if __name__ == "__main__":
    test_ranked_counts_breaks_ties_by_name()
    print("compare-report generator tests passed")
