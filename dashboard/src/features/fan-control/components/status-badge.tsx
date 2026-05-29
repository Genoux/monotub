import { Badge } from "@/components/ui/badge";
import { cn } from "@/lib/utils";

type StatusBadgeProps = {
  label: string;
  variant: "on" | "off" | "offline" | "unknown";
};

const variantClasses: Record<StatusBadgeProps["variant"], string> = {
  on: "border-[var(--status-on)]/40 bg-[var(--status-on)]/15 text-[var(--status-on)]",
  off: "border-[var(--status-off)]/40 bg-[var(--status-off)]/15 text-[var(--status-off)]",
  offline:
    "border-[var(--status-offline)]/40 bg-[var(--status-offline)]/15 text-[var(--status-offline)]",
  unknown: "border-border bg-muted text-muted-foreground",
};

export const StatusBadge = ({ label, variant }: StatusBadgeProps) => (
  <Badge
    variant="outline"
    className={cn(
      "rounded-none font-mono text-[10px] uppercase tracking-widest",
      variantClasses[variant],
    )}
  >
    {label}
  </Badge>
);
