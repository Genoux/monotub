"use client";

import type { ReactNode } from "react";
import { AppSidebar } from "@/components/app-sidebar";
import {
  SidebarInset,
  SidebarProvider,
  SidebarTrigger,
} from "@/components/ui/sidebar";
import { Separator } from "@/components/ui/separator";

type DashboardShellProps = {
  children: ReactNode;
};

export const DashboardShell = ({ children }: DashboardShellProps) => (
  <SidebarProvider defaultOpen>
    <AppSidebar />
    <SidebarInset className="rounded-none">
      <header className="flex h-12 shrink-0 items-center gap-2 border-b border-border px-4">
        <SidebarTrigger className="-ml-1 rounded-none" />
        <Separator orientation="vertical" className="h-4" />
        <span className="font-mono text-xs uppercase tracking-widest text-muted-foreground">
          Dashboard
        </span>
      </header>
      <main className="flex-1 p-4 md:p-6">{children}</main>
    </SidebarInset>
  </SidebarProvider>
);
