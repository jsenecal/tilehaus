// Cities the Clock card (type 27) offers, with their POSIX TZ rules. The panel
// has no time zone database, so a world clock stores the rule itself (ESPHome's
// parser applies it on the device). "" = the panel's own time zone, from HA.
//
// Rules as of 2026: Mexico City dropped DST in 2022; others follow their
// current law. A place that changes its rules needs its line updated here.

export interface TimeZoneOption {
  readonly city: string;
  readonly tz: string;
}

export const TIME_ZONES: readonly TimeZoneOption[] = [
  { city: "Panel's own time", tz: "" },
  { city: "UTC", tz: "UTC0" },
  { city: "Honolulu", tz: "HST10" },
  { city: "Anchorage", tz: "AKST9AKDT,M3.2.0,M11.1.0" },
  { city: "Los Angeles", tz: "PST8PDT,M3.2.0,M11.1.0" },
  { city: "Vancouver", tz: "PST8PDT,M3.2.0,M11.1.0" },
  { city: "Phoenix", tz: "MST7" },
  { city: "Denver", tz: "MST7MDT,M3.2.0,M11.1.0" },
  { city: "Mexico City", tz: "CST6" },
  { city: "Chicago", tz: "CST6CDT,M3.2.0,M11.1.0" },
  { city: "New York", tz: "EST5EDT,M3.2.0,M11.1.0" },
  { city: "Toronto", tz: "EST5EDT,M3.2.0,M11.1.0" },
  { city: "Montreal", tz: "EST5EDT,M3.2.0,M11.1.0" },
  { city: "Halifax", tz: "AST4ADT,M3.2.0,M11.1.0" },
  { city: "St. John's", tz: "NST3:30NDT,M3.2.0,M11.1.0" },
  { city: "São Paulo", tz: "<-03>3" },
  { city: "Buenos Aires", tz: "<-03>3" },
  { city: "Reykjavik", tz: "GMT0" },
  { city: "Lisbon", tz: "WET0WEST,M3.5.0/1,M10.5.0" },
  { city: "London", tz: "GMT0BST,M3.5.0/1,M10.5.0" },
  { city: "Paris", tz: "CET-1CEST,M3.5.0,M10.5.0/3" },
  { city: "Berlin", tz: "CET-1CEST,M3.5.0,M10.5.0/3" },
  { city: "Madrid", tz: "CET-1CEST,M3.5.0,M10.5.0/3" },
  { city: "Rome", tz: "CET-1CEST,M3.5.0,M10.5.0/3" },
  { city: "Johannesburg", tz: "SAST-2" },
  { city: "Athens", tz: "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { city: "Helsinki", tz: "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { city: "Istanbul", tz: "<+03>-3" },
  { city: "Moscow", tz: "MSK-3" },
  { city: "Dubai", tz: "<+04>-4" },
  { city: "Mumbai", tz: "IST-5:30" },
  { city: "Bangkok", tz: "<+07>-7" },
  { city: "Jakarta", tz: "WIB-7" },
  { city: "Singapore", tz: "<+08>-8" },
  { city: "Hong Kong", tz: "HKT-8" },
  { city: "Shanghai", tz: "CST-8" },
  { city: "Tokyo", tz: "JST-9" },
  { city: "Seoul", tz: "KST-9" },
  { city: "Sydney", tz: "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { city: "Auckland", tz: "NZST-12NZDT,M9.5.0,M4.1.0/3" },
];

/** The city for a stored rule, or null for a rule not in the list (typed by hand). */
export function cityForTz(tz: string): string | null {
  return TIME_ZONES.find((z) => z.tz === tz)?.city ?? null;
}
