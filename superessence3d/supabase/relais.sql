-- SUPERESSENCE 3D Mobile : relais Claude (MCP) ↔ téléphone.
-- À coller dans Supabase › SQL Editor › New query, puis « Run » (une seule fois).
create table if not exists public.se_devices (
  secret text primary key,
  created_at timestamptz not null default now(),
  last_seen timestamptz
);
create table if not exists public.se_commands (
  id bigint generated always as identity primary key,
  secret text not null references public.se_devices(secret) on delete cascade,
  tool text not null,
  args jsonb not null default '{}'::jsonb,
  status text not null default 'pending',   -- pending | running | done | error
  result jsonb,
  created_at timestamptz not null default now(),
  done_at timestamptz
);
create index if not exists se_commands_wait on public.se_commands (secret, status, id);
-- Aucun accès direct depuis l'extérieur : tout passe par la fonction (clé de service côté serveur).
alter table public.se_devices enable row level security;
alter table public.se_commands enable row level security;
-- Nettoyage : les commandes de plus d'un jour sont supprimées à chaque nouvelle commande (voir la fonction).
