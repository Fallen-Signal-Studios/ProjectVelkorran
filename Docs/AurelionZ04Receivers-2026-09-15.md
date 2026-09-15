# Z04 custom relay receiver terminals

The two native receiver actors receive a custom Aurelion terminal mesh: ivory cheeks and crown, recessed dark screen, narrow gold contacts, physical isolation lever, service cover, captive fasteners, side cooling fins and rear cable sockets. The mesh is 90 x 60 x 131 cm, with 11,160 triangles, two UV channels, no collision, Nanite precision 10 and full fallback geometry. Its unit-scale placement matches the native box footprint and top, extending one centimetre below the box to seat on the floor.

Four overlapping legacy receiver-console and column instances are removed from their existing decorative batches. Each batch retains its third scanner-prop instance at the exact original pose. The receiver actor transforms, Body boxes, component parents, labels, receiver IDs, encounter references, 250 cm interaction distance and 0.5 second hold time remain unchanged. No native C++ payload or journal behavior is modified.

The status material reads `ReceiverDisabled` from custom primitive data index 0. Native `ASovCampaignRelayReceiver::RefreshPresentation` writes that index and updates the receiver label; the mesh's status apertures interpolate from cyan emission to a dim dark state. This adds presentation around the existing state owner.

## Evidence and limits

Fresh device inventory `Z04DevicesBaseline-20260915-004902-c19b3bba` exited 0 without Python errors. It records both receivers, their overlapping art batches and the existing Z03 sweep scanners. The native sensor actors are in Z03; the remaining Z04 scanner-shaped prop is not evidence of an active Z04 sensor.

The final clean-FBX check passed, the scoped coplanar audit found zero same-facing axis-aligned convex overlaps, and the Blender studio render was inspected. Runtime API metadata confirmed the scalar parameter's custom primitive data properties. The first engine preview reached the material graph check but failed because that checker used a direct Python attribute instead of `get_editor_property`; this was corrected before retrying. That failed run did not save the campaign map.

Corrected preview `Z04ReceiversPreviewVerified-20260915-010032-e26b1e56` exited 0 without Python errors. All three views were inspected: east active, west shader-disabled, and room context. The status bars visibly change from bright cyan to a dim screen. Both terminals are seated on the floor, and the overlapping legacy receiver props are removed. The editor's native label still reads its default `Text`; native presentation replaces that text at BeginPlay.

Saved run `Z04ReceiversSaved-20260915-010323-92300317` exited 0 without Python errors; all three saved views were inspected and the map was backed up before saving. Fresh saved-map run `Z04ReceiversFresh-20260915-010628-95ae0a6a` exited 0 without Python errors. All 85 architecture reports passed with 3140 actors and a clean map, including the receiver bindings and material graph. The stopped-editor capture deliberately sets the west receiver's primitive data to 1 for the dimmed shader comparison, after any map save, then restores it to 0 during capture cleanup. This is a cosmetic shader test and does not write or prove a gameplay disable receipt. Actual interaction, disable, retry and checkpoint restoration still require live qualification.

Remaining work includes the Z04 scanner prop, piers, lighting, full-map visual acceptance, supporting cast and campaign destruction. The supported slice estimate remains 63.75%.
