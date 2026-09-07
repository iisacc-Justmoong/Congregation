# Congregation external-platform launch kit

This document contains factual, reusable English copy for Congregation listings on
platforms other than iisacc.com. It must not be changed to claim that a signed
Windows installer is publicly available until a Microsoft Store listing or a
publicly trusted Authenticode-signed release has been verified.

## Canonical facts

- Product: Congregation 1.0.0
- Current source release: 1.0.0
- Category: raster drawing, digital paper, handwriting, note-taking
- Platforms: Windows, macOS, and Linux source/build support
- License: GNU AGPLv3
- Distribution: version 1.0.0 source release under GNU AGPLv3; a commercial Congregation listing and signed installers are separate rollout steps
- Privacy: account email and license key are used only for online license validation and a user-requested update grant; opening Preferences may explicitly read the stored email for an email-only fixed display. There is no telemetry, analytics, advertising, cloud document upload, remote polling, or automatic/background update check. When the user-controlled nearby-discovery setting is enabled, Congregation exchanges an ephemeral anonymous presence heartbeat only within the current LAN; it includes no profile, account, device name, or document data, only an invitation-capability Boolean and a temporary TCP port while the user explicitly shares a canvas. Selecting an opted-in nearby user from Members sends that target a one-hop invitation containing the inviter's profile name and temporary canvas endpoint. After acceptance or an explicit join, participants send bounded edit commands to the host-owned canvas and only the host returns complete authoritative canvas snapshots directly over the LAN without an Internet relay
- Public project URL: https://github.com/iisacc-Justmoong/Congregation
- Source release URL:
  https://github.com/iisacc-Justmoong/Congregation/releases/tag/v1.0.0
- Feedback URL:
  https://github.com/iisacc-Justmoong/Congregation/discussions
- Windows testing URL:
  https://github.com/iisacc-Justmoong/Congregation/issues/new

## Tagline

Private, local-first digital paper for focused drawing.

## Short description

Congregation is a local-first raster drawing app for focused drawing, handwriting,
layered editing, and PSD-compatible workflows. A purchased iisacc account
license unlocks the canvas through online verification, while documents remain
local with no telemetry, advertising, remote polling, or automatic update
checks. An optional anonymous one-hop local-network heartbeat detects other
nearby Congregation devices without sending profile, account, device-name, or
document data. Users can explicitly share or join a canvas, or invite a
specifically selected opted-in nearby user. Only that explicit invitation adds
the inviter's profile name and endpoint; accepted sessions transfer profile
names and bounded edit commands to the host-owned canvas; only the host returns
authoritative canvas snapshots directly to participating LAN devices, without a
cloud relay.

## Directory summary

Congregation 1.0.0 is a Qt 6 desktop raster editor for drawing, handwriting, layered
canvas work, and PSD-compatible import and export. Its local-first design keeps
documents on the user's device; the app sends account-license credentials only
for activation and a user-requested update, with no telemetry, advertising, or
automatic update checks. Its user-controlled one-hop LAN presence heartbeat is
anonymous and contains no profile, account, device-name, or document data, only
an invitation-capability Boolean and a temporary port while canvas sharing is
active. A specifically targeted invitation adds the inviter's profile name only
after a Members `+` selection; canvas and participant data move only after
acceptance or explicit share/join actions and remain direct between participating
LAN devices.
The source is available under GNU AGPLv3.
A publicly trusted Windows package is in preparation.

## Product Hunt description

Congregation is a local-first raster drawing app for handwriting, layered artwork,
and PSD-compatible workflows. A purchased license is checked online while
documents stay on the device with no telemetry or ads. AGPLv3 source is
available now; an optional anonymous one-hop LAN beacon can detect another nearby
Congregation device, and a signed Windows release is in preparation.

## First launch comment

I built Congregation because a drawing surface should feel like paper, not a
service. It is a native Qt 6 desktop application with pressure-aware brushes,
layers, shapes, text, image import, and PSD-compatible workflows. Documents
stay local. Congregation uses the purchaser's iisacc account email and license key
only for activation or a user-requested update, and has no telemetry, advertising, or automatic update checks.
Nearby Congregation discovery is confined to an anonymous one-hop LAN heartbeat and
contains no profile, account, device-name, or document data, only an invitation
capability and a temporary port while canvas sharing is active. Selecting an
opted-in nearby user can send that target the inviter's profile name and endpoint;
accepted or explicitly joined devices send profile names and bounded edit commands
to the host-owned canvas; only the host returns authoritative snapshots directly
over that LAN, without a cloud relay.

Version 1.0.0 is available as complete AGPLv3 source. We are currently
preparing the publicly trusted Windows distribution and would especially value
feedback from artists, pen-tablet users, Qt developers, and Windows testers.

## Suggested tags

- Drawing
- Digital Art
- Handwriting
- Note-taking
- Open Source
- Privacy
- Raster Graphics
- Qt
- Windows

## Suggested alternatives

When a platform requests comparable products, use only products that share the
same primary workflow:

- Krita
- MyPaint
- Microsoft Paint
- Paint.NET
- Sketchbook

Do not describe Congregation as a complete replacement for these products. Present
it as a focused, local-first alternative.

## Asset inventory

- `congregation-windows-editor.png`: verified screenshot of the running Windows
  application, suitable as the primary gallery image.
- `congregation-sample-artwork.png`: generated demonstration artwork opened in the
  application screenshot. Use it only as a secondary example image and never
  present it as application UI.

## Availability wording

Use this sentence until a trusted installer is public:

> Complete source is available now. A publicly trusted Windows installer is
> in preparation and development-only self-signed packages are not
> distributed.

After a trusted release is verified, replace the sentence with a direct
platform download link and the exact verified publisher identity. Do not
promise that Microsoft Defender SmartScreen will never warn because reputation
is evaluated separately from Authenticode validity.
