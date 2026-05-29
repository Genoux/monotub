import { Fan, LayoutDashboard } from "lucide-react";
import type { LucideIcon } from "lucide-react";

export type NavItem = {
  title: string;
  href: string;
  icon: LucideIcon;
  disabled?: boolean;
};

export const mainNavigation: NavItem[] = [
  {
    title: "Fan control",
    href: "/",
    icon: Fan,
  },
];

export const futureNavigation: NavItem[] = [
  {
    title: "Overview",
    href: "/overview",
    icon: LayoutDashboard,
    disabled: true,
  },
];
