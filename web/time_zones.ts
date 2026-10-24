// Cities the Clock card (type 27) offers, with their POSIX TZ rules. The panel
// has no time zone database, so a world clock stores the rule itself (ESPHome's
// parser applies it on the device). "" = the panel's own time zone, from HA.
//
// Rules as of 2026: Mexico City dropped DST in 2022; others follow their
// current law. A place that changes its rules needs its line updated here.

export interface TimeZoneOption {
  readonly city: string;
  readonly tz: string;    // POSIX rule, what the panel stores and applies
  readonly iana: string;  // the same zone for the browser (the configurator's preview)
}

export const TIME_ZONES: readonly TimeZoneOption[] = [
  { city: "Panel's own time", tz: "", iana: "" },
  { city: "UTC", tz: "UTC0", iana: "UTC" },
  { city: "Honolulu", tz: "HST10", iana: "Pacific/Honolulu" },
  { city: "Anchorage", tz: "AKST9AKDT,M3.2.0,M11.1.0", iana: "America/Anchorage" },
  { city: "Los Angeles", tz: "PST8PDT,M3.2.0,M11.1.0", iana: "America/Los_Angeles" },
  { city: "Vancouver", tz: "PST8PDT,M3.2.0,M11.1.0", iana: "America/Vancouver" },
  { city: "Phoenix", tz: "MST7", iana: "America/Phoenix" },
  { city: "Denver", tz: "MST7MDT,M3.2.0,M11.1.0", iana: "America/Denver" },
  { city: "Mexico City", tz: "CST6", iana: "America/Mexico_City" },
  { city: "Chicago", tz: "CST6CDT,M3.2.0,M11.1.0", iana: "America/Chicago" },
  { city: "New York", tz: "EST5EDT,M3.2.0,M11.1.0", iana: "America/New_York" },
  { city: "Toronto", tz: "EST5EDT,M3.2.0,M11.1.0", iana: "America/Toronto" },
  { city: "Montreal", tz: "EST5EDT,M3.2.0,M11.1.0", iana: "America/Montreal" },
  { city: "Halifax", tz: "AST4ADT,M3.2.0,M11.1.0", iana: "America/Halifax" },
  { city: "St. John's", tz: "NST3:30NDT,M3.2.0,M11.1.0", iana: "America/St_Johns" },
  { city: "São Paulo", tz: "<-03>3", iana: "America/Sao_Paulo" },
  { city: "Buenos Aires", tz: "<-03>3", iana: "America/Argentina/Buenos_Aires" },
  { city: "Reykjavik", tz: "GMT0", iana: "Atlantic/Reykjavik" },
  { city: "Lisbon", tz: "WET0WEST,M3.5.0/1,M10.5.0", iana: "Europe/Lisbon" },
  { city: "London", tz: "GMT0BST,M3.5.0/1,M10.5.0", iana: "Europe/London" },
  { city: "Paris", tz: "CET-1CEST,M3.5.0,M10.5.0/3", iana: "Europe/Paris" },
  { city: "Berlin", tz: "CET-1CEST,M3.5.0,M10.5.0/3", iana: "Europe/Berlin" },
  { city: "Madrid", tz: "CET-1CEST,M3.5.0,M10.5.0/3", iana: "Europe/Madrid" },
  { city: "Rome", tz: "CET-1CEST,M3.5.0,M10.5.0/3", iana: "Europe/Rome" },
  { city: "Johannesburg", tz: "SAST-2", iana: "Africa/Johannesburg" },
  { city: "Athens", tz: "EET-2EEST,M3.5.0/3,M10.5.0/4", iana: "Europe/Athens" },
  { city: "Helsinki", tz: "EET-2EEST,M3.5.0/3,M10.5.0/4", iana: "Europe/Helsinki" },
  { city: "Istanbul", tz: "<+03>-3", iana: "Europe/Istanbul" },
  { city: "Moscow", tz: "MSK-3", iana: "Europe/Moscow" },
  { city: "Dubai", tz: "<+04>-4", iana: "Asia/Dubai" },
  { city: "Mumbai", tz: "IST-5:30", iana: "Asia/Kolkata" },
  { city: "Bangkok", tz: "<+07>-7", iana: "Asia/Bangkok" },
  { city: "Jakarta", tz: "WIB-7", iana: "Asia/Jakarta" },
  { city: "Singapore", tz: "<+08>-8", iana: "Asia/Singapore" },
  { city: "Hong Kong", tz: "HKT-8", iana: "Asia/Hong_Kong" },
  { city: "Shanghai", tz: "CST-8", iana: "Asia/Shanghai" },
  { city: "Tokyo", tz: "JST-9", iana: "Asia/Tokyo" },
  { city: "Seoul", tz: "KST-9", iana: "Asia/Seoul" },
  { city: "Sydney", tz: "AEST-10AEDT,M10.1.0,M4.1.0/3", iana: "Australia/Sydney" },
  { city: "Auckland", tz: "NZST-12NZDT,M9.5.0,M4.1.0/3", iana: "Pacific/Auckland" },
];

/** The city for a stored rule, or null for a rule not in the list (typed by hand). */
export function cityForTz(tz: string): string | null {
  return TIME_ZONES.find((z) => z.tz === tz)?.city ?? null;
}
