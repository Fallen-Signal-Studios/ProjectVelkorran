# Aurelion Graybox

The Aurelion level graybox is available at:

`/Game/Aurelion/Maps/L_Aurelion_Graybox_Velkorran`

This repository copy contains the playable spatial layout, graybox materials, and the three low-poly ring-sector meshes used by the level. Its map-level game mode override is intentionally cleared so Project Velkorran supplies its own configured game mode and player systems. The stock Unreal Third Person template and mannequin content are therefore not required by this map.

The source layout was built and validated in Unreal Engine 5.7.4 from `Aurelion_Level_Layout_Plan.pdf`. It contains all 13 planned zones and their traversal connections. Collision checks covered the critical platforms and routes, and a playable character crossed the first connector successfully in the source project.

The selected Unreal binary assets are committed directly because this self-contained graybox payload is small. Generated Unreal content and raw source art remain excluded.
