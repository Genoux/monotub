"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { Terminal } from "lucide-react";
import {
  Sidebar,
  SidebarContent,
  SidebarFooter,
  SidebarGroup,
  SidebarGroupContent,
  SidebarGroupLabel,
  SidebarHeader,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
  SidebarRail,
} from "@/components/ui/sidebar";
import { futureNavigation, mainNavigation } from "@/config/navigation";

export const AppSidebar = () => {
  const pathname = usePathname();

  return (
    <Sidebar
      collapsible="icon"
      className="rounded-none border-r border-sidebar-border"
    >
      <SidebarHeader className="border-b border-sidebar-border">
        <div className="flex items-center gap-2 px-2 py-1">
          <SidebarIcon />
          <div className="group-data-[collapsible=icon]:hidden">
            <p className="font-mono text-sm font-semibold tracking-tight">
              Monotub
            </p>
            <p className="font-mono text-[10px] uppercase tracking-widest text-muted-foreground">
              Control panel
            </p>
          </div>
        </div>
      </SidebarHeader>
      <SidebarContent>
        <SidebarGroup>
          <SidebarGroupLabel className="font-mono text-[10px] uppercase tracking-widest">
            Features
          </SidebarGroupLabel>
          <SidebarGroupContent>
            <SidebarMenu>
              {mainNavigation.map((item) => (
                <SidebarMenuItem key={item.href}>
                  <SidebarMenuButton
                    asChild
                    isActive={pathname === item.href}
                    tooltip={item.title}
                    className="rounded-none font-mono text-xs"
                  >
                    <Link href={item.href}>
                      <item.icon />
                      <span>{item.title}</span>
                    </Link>
                  </SidebarMenuButton>
                </SidebarMenuItem>
              ))}
            </SidebarMenu>
          </SidebarGroupContent>
        </SidebarGroup>
        <SidebarGroup>
          <SidebarGroupLabel className="font-mono text-[10px] uppercase tracking-widest">
            Coming soon
          </SidebarGroupLabel>
          <SidebarGroupContent>
            <SidebarMenu>
              {futureNavigation.map((item) => (
                <SidebarMenuItem key={item.href}>
                  <SidebarMenuButton
                    disabled={item.disabled}
                    tooltip={item.title}
                    className="rounded-none font-mono text-xs"
                  >
                    <item.icon />
                    <span>{item.title}</span>
                  </SidebarMenuButton>
                </SidebarMenuItem>
              ))}
            </SidebarMenu>
          </SidebarGroupContent>
        </SidebarGroup>
      </SidebarContent>
      <SidebarFooter className="border-t border-sidebar-border p-3">
        <p className="font-mono text-[10px] uppercase tracking-widest text-muted-foreground">
          monotub v0.1
        </p>
      </SidebarFooter>
      <SidebarRail />
    </Sidebar>
  );
};

const SidebarIcon = () => (
  <div className="flex size-8 items-center justify-center border border-sidebar-border bg-sidebar-accent">
    <Terminal className="size-4" />
  </div>
);
