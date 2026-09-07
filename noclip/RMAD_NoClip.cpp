#include <windows.h>
#include <cmath>
#include "main.h"
#include "natives.h"

static bool g_enabled = false;
static Entity g_entity = 0;

static Vector3 add3(const Vector3& a, const Vector3& b) {
    Vector3 r{}; r.x=a.x+b.x; r.y=a.y+b.y; r.z=a.z+b.z; return r;
}
static Vector3 mul3(const Vector3& v, float s) {
    Vector3 r{}; r.x=v.x*s; r.y=v.y*s; r.z=v.z*s; return r;
}

static float normalizeHeading(float h) {
    while (h >= 360.0f) h -= 360.0f;
    while (h < 0.0f) h += 360.0f;
    return h;
}

static void camBasis(Vector3& forward, Vector3& right) {
    const Vector3 rot = CAM::GET_GAMEPLAY_CAM_ROT(2);
    const float rz = rot.z * 0.01745329251994329577f;
    const float rx = rot.x * 0.01745329251994329577f;
    const float cosx = std::cos(rx);
    forward.x = -std::sin(rz) * cosx;
    forward.y =  std::cos(rz) * cosx;
    forward.z =  std::sin(rx);
    right.x = std::cos(rz);
    right.y = std::sin(rz);
    right.z = 0.0f;
}

static Entity currentEntity() {
    Ped ped = PLAYER::PLAYER_PED_ID();
    if (PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE))
        return PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
    return ped;
}

static void restoreEntity(Entity e) {
    if (!ENTITY::DOES_ENTITY_EXIST(e)) return;
    ENTITY::SET_ENTITY_COLLISION(e, TRUE, TRUE);
    ENTITY::SET_ENTITY_HAS_GRAVITY(e, TRUE);
    ENTITY::SET_ENTITY_VELOCITY(e, 0.0f, 0.0f, 0.0f);
}

static void disableNoClip() {
    if (g_entity) restoreEntity(g_entity);
    g_entity = 0;
    g_enabled = false;
}

static void keepWeaponControlsEnabled() {
    // Keep aiming/firing/reloading/weapon switching available while flying.
    PAD::ENABLE_CONTROL_ACTION(0, 24, TRUE); // INPUT_ATTACK
    PAD::ENABLE_CONTROL_ACTION(0, 25, TRUE); // INPUT_AIM
    PAD::ENABLE_CONTROL_ACTION(0, 37, TRUE); // INPUT_SELECT_WEAPON
    PAD::ENABLE_CONTROL_ACTION(0, 45, TRUE); // INPUT_RELOAD
    PAD::ENABLE_CONTROL_ACTION(0, 257, TRUE); // INPUT_ATTACK2
    PAD::ENABLE_CONTROL_ACTION(0, 263, TRUE); // INPUT_MELEE_ATTACK1
    PAD::ENABLE_CONTROL_ACTION(0, 264, TRUE); // INPUT_MELEE_ATTACK2
}

static void tickNoClip() {
    Ped ped = PLAYER::PLAYER_PED_ID();
    Entity e = currentEntity();
    if (g_entity && g_entity != e) restoreEntity(g_entity);
    g_entity = e;
    if (!ENTITY::DOES_ENTITY_EXIST(e)) return;

    ENTITY::SET_ENTITY_COLLISION(e, FALSE, FALSE);
    ENTITY::SET_ENTITY_HAS_GRAVITY(e, FALSE);
    ENTITY::SET_ENTITY_VELOCITY(e, 0.0f, 0.0f, 0.0f);

    // Do not let noclip suppress normal weapon controls.
    keepWeaponControlsEnabled();

    Vector3 pos = ENTITY::GET_ENTITY_COORDS(e, TRUE);
    Vector3 forward{}, right{};
    camBasis(forward, right);

    float speed = 0.9f;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) speed = 3.0f;
    if (GetAsyncKeyState(VK_MENU) & 0x8000)  speed = 0.25f;

    Vector3 delta{};
    if (GetAsyncKeyState('W') & 0x8000) delta = add3(delta, mul3(forward, speed));
    if (GetAsyncKeyState('S') & 0x8000) delta = add3(delta, mul3(forward, -speed));
    if (GetAsyncKeyState('D') & 0x8000) delta = add3(delta, mul3(right, speed));
    if (GetAsyncKeyState('A') & 0x8000) delta = add3(delta, mul3(right, -speed));
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) delta.z += speed;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) delta.z -= speed;

    pos = add3(pos, delta);
    ENTITY::SET_ENTITY_COORDS_NO_OFFSET(e, pos.x, pos.y, pos.z, FALSE, FALSE, FALSE);

    // Keep the player's back toward the camera during normal flight.
    // While aiming/shooting, let GTA control the ped's aiming orientation.
    const bool aiming = PAD::IS_CONTROL_PRESSED(0, 25) || PAD::IS_CONTROL_PRESSED(0, 24);
    if (!aiming && e == ped &&
        ((GetAsyncKeyState('W') | GetAsyncKeyState('S') | GetAsyncKeyState('A') | GetAsyncKeyState('D')) & 0x8000)) {
        const Vector3 rot = CAM::GET_GAMEPLAY_CAM_ROT(2);
        ENTITY::SET_ENTITY_HEADING(e, normalizeHeading(rot.z + 180.0f));
    }
}

void ScriptMain() {
    while (true) {
        if (GetAsyncKeyState(VK_F2) & 1) {
            if (g_enabled) disableNoClip();
            else g_enabled = true;
        }
        if (g_enabled) tickNoClip();
        scriptWait(0);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        scriptRegister(hModule, ScriptMain);
    } else if (reason == DLL_PROCESS_DETACH) {
        disableNoClip();
        scriptUnregister(hModule);
    }
    return TRUE;
}
