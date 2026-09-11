// Frozen DECK wire enum (must match poc/native/card_config.h — never renumber).
export interface CardTypeOption {
  readonly value: number;
  readonly label: string;
}

export const CARD_TYPES: readonly CardTypeOption[] = [
  { value: 0, label: "Blank" },
  { value: 1, label: "Sensor" },
  { value: 2, label: "Toggle" },
  { value: 3, label: "Light (slider)" },
  { value: 4, label: "Light (control)" },
  { value: 5, label: "Cover" },
  { value: 6, label: "Presence" },
  { value: 7, label: "Weather" },
  { value: 8, label: "Scene" },
  { value: 9, label: "Button" },
  { value: 10, label: "Lock" },
  { value: 11, label: "Header" },
  { value: 12, label: "Climate" },
  { value: 13, label: "WLED" },
  { value: 14, label: "Door/Window" },
  { value: 15, label: "Number slider" },
  { value: 16, label: "Option select" },
  { value: 17, label: "Fan" },
  { value: 18, label: "Alarm" },
  { value: 19, label: "Weather forecast" },
  // value 20 (Camera) is reserved but not offered — the card was removed.
  { value: 21, label: "Page" },
  { value: 22, label: "Back" },
];

export function cardTypeLabel(value: number): string {
  return CARD_TYPES.find((option) => option.value === value)?.label ?? `Type ${value}`;
}
