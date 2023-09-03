/*
    Copyright (c) 2021-2023 Jaedeok Kim <jdeokkim@protonmail.com>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/* Includes ============================================================================= */

#include "ferox.h"

/* Typedefs ============================================================================= */

/* A structure that represents the motion data of a rigid body. */
typedef struct _frMotionData {
    float mass, inverseMass;
    float inertia, inverseInertia;
    float gravityScale;
    frVector2 velocity;
    float angularVelocity;
    frVector2 force;
    float torque;
} frMotionData;

/* A structure that represents a rigid body. */
struct _frBody {
    frBodyType type;
    frBodyFlags flags;
    frShape *shape;
    frTransform tx;
    frMotionData mtn;
    frAABB aabb;
    void *ctx;
};

/* Constants ============================================================================ */

/* Constants for `frNormalizeAngle()`. */
static const float TWO_PI = 2.0f * M_PI, INVERSE_TWO_PI = 1.0f / (2.0f * M_PI);

/* Private Function Prototypes ========================================================== */

/* Computes the mass and the moment of inertia for `b`. */
static void frComputeBodyMass(frBody *b);

/* Normalizes the `angle` to a range `[0, 2π]`. */
static FR_API_INLINE float frNormalizeAngle(float angle);

/* Public Functions ===================================================================== */

/* Creates a rigid body at `position`. */
frBody *frCreateBody(frBodyType type, frVector2 position) {
    if (type < FR_BODY_STATIC || type > FR_BODY_DYNAMIC) return NULL;

    frBody *result = calloc(1, sizeof *result);

    result->type = type;

    result->tx.position = position;

    result->tx.rotation._sin = 0.0f;
    result->tx.rotation._cos = 1.0f;

    result->mtn.gravityScale = 1.0f;
    
    return result;
}

/* Creates a rigid body at `position`, then attaches `s` to it. */
frBody *frCreateBodyFromShape(frBodyType type, frVector2 position, frShape *s) {
    frBody *result = frCreateBody(type, position);

    frSetBodyShape(result, s);

    return result;
}

/* Releases the memory allocated for `b`. */
void frReleaseBody(frBody *b) {
    free(b);
}

/* Returns the type of `b`. */
frBodyType frGetBodyType(const frBody *b) {
    return (b != NULL) ? b->type : FR_BODY_UNKNOWN;
}

/* Returns the property flags of `b`. */
frBodyFlags frGetBodyFlags(const frBody *b) {
    return (b != NULL) ? b->flags : 0u;
}

/* Returns the collision shape of `b`. */
frShape *frGetBodyShape(const frBody *b) {
    return (b != NULL) ? b->shape : NULL;
}

/* Returns the transform of `b`. */
frTransform frGetBodyTransform(const frBody *b) {
    return (b != NULL) ? b->tx : FR_API_STRUCT_ZERO(frTransform);
}

/* Returns the position of `b`. */
frVector2 frGetBodyPosition(const frBody *b) {
    return (b != NULL) ? b->tx.position : FR_API_STRUCT_ZERO(frVector2);
}

/* Returns the angle of `b`, in radians. */
float frGetBodyAngle(const frBody *b) {
    return (b != NULL) ? b->tx.angle : 0.0f;
}

/* Returns the mass of `b`. */
float frGetBodyMass(const frBody *b) {
    return (b != NULL) ? b->mtn.mass : 0.0f;
}

/* Returns the inverse mass of `b`. */
float frGetBodyInverseMass(const frBody *b) {
    return (b != NULL) ? b->mtn.inverseMass : 0.0f;
}

/* Returns the moment of inertia of `b`. */
float frGetBodyInertia(const frBody *b) {
    return (b != NULL) ? b->mtn.inertia : 0.0f;
}

/* Returns the inverse moment of inertia of `b`. */
float frGetBodyInverseInertia(const frBody *b) {
    return (b != NULL) ? b->mtn.inverseInertia : 0.0f;
}

/* Returns the gravity scale of `b`. */
float frGetBodyGravityScale(const frBody *b) {
    return (b != NULL) ? b->mtn.gravityScale : 0.0f;
}

/* Returns the velocity of `b`. */
frVector2 frGetBodyVelocity(const frBody *b) {
    return (b != NULL) ? b->mtn.velocity : FR_API_STRUCT_ZERO(frVector2);
}

/* Returns the AABB (Axis-Aligned Bounding Box) of `b`. */
frAABB frGetBodyAABB(const frBody *b) {
    return (b != NULL && b->shape != NULL) ? b->aabb : FR_API_STRUCT_ZERO(frAABB);
}

/* Returns the user data of `b`. */
void *frGetBodyUserData(const frBody *b) {
    return (b != NULL) ? b->ctx : NULL;
}

/* Sets the `type` of `b`. */
void frSetBodyType(frBody *b, frBodyType type) {
    if (b == NULL) return;

    b->type = type;

    frComputeBodyMass(b);
}

/* Sets the property `flags` of `b`. */
void frSetBodyFlags(frBody *b, frBodyFlags flags) {
    if (b == NULL) return;

    b->flags = flags;

    frComputeBodyMass(b);
}

/* 
    Attaches the collision `s`hape to `b`. If `s` is `NULL`, 
    it will detach the current collision shape from `b`.
*/
void frSetBodyShape(frBody *b, frShape *s) {
    if (b == NULL) return;

    b->shape = s;

    b->aabb = (s != NULL) ? frGetShapeAABB(s, b->tx) : FR_API_STRUCT_ZERO(frAABB);
    
    frComputeBodyMass(b);
}

/* Sets the position of `b` to `position`. */
void frSetBodyPosition(frBody *b, frVector2 position) {
    if (b == NULL) return;

    b->tx.position = position;

    b->aabb = frGetShapeAABB(b->shape, b->tx);
}

/* Sets the `angle` of `b`, in radians. */
void frSetBodyAngle(frBody *b, float angle) {
    if (b == NULL) return;

    b->tx.angle = frNormalizeAngle(angle);

    /*
        NOTE: These values must be cached in order to 
        avoid expensive computations as much as possible.
    */
    b->tx.rotation._sin = sinf(b->tx.angle);
    b->tx.rotation._cos = cosf(b->tx.angle);

    b->aabb = frGetShapeAABB(b->shape, b->tx);
}

/* Sets the gravity `scale` of `b`. */
void frSetBodyGravityScale(frBody *b, float scale) {
    if (b != NULL) b->mtn.gravityScale = scale;
}

/* Sets the `velocity` of `b`. */
void frSetBodyVelocity(frBody *b, frVector2 velocity) {
    if (b != NULL) b->mtn.velocity = velocity;
}

/* Sets the `angularVelocity` of `b`. */
void frSetBodyAngularVelocity(frBody *b, float angularVelocity) {
    if (b != NULL) b->mtn.angularVelocity = angularVelocity;
}

/* Sets the user data of `b` to `ctx`. */
void frSetBodyUserData(frBody *b, void *ctx) {
    if (b != NULL) b->ctx = ctx;
}

/* Clears accumulated forces on `b`. */
void frClearBodyForces(frBody *b) {
    if (b == NULL) return;

    b->mtn.force.x = b->mtn.force.y = b->mtn.torque = 0.0f;
}

/* Applies a `force` at a `point` on `b`. */
void frApplyForceToBody(frBody *b, frVector2 point, frVector2 force) {
    if (b == NULL || b->mtn.inverseMass <= 0.0f) return;

    b->mtn.force = frVector2Add(b->mtn.force, force);
    b->mtn.torque += frVector2Cross(point, force);
}

/* Applies a gravity force to `b` with the `g`ravity acceleration vector. */
void frApplyGravityToBody(frBody *b, frVector2 g) {
    if (b == NULL || b->mtn.mass <= 0.0f) return;
    
    b->mtn.force = frVector2Add(
        b->mtn.force,
        frVector2ScalarMultiply(g, b->mtn.gravityScale * b->mtn.mass)
    );
}

/* Applies an `impulse` at a `point` on `b`. */
void frApplyImpulseToBody(frBody *b, frVector2 point, frVector2 impulse) {
    if (b == NULL || b->mtn.inverseMass <= 0.0f) return;

    b->mtn.velocity = frVector2Add(
        b->mtn.velocity,
        frVector2ScalarMultiply(impulse, b->mtn.inverseMass)
    );

    b->mtn.angularVelocity += b->mtn.inverseInertia 
        * frVector2Cross(point, impulse);
}

/* 
    Calculates the acceleration of `b` from the accumulated forces,
    then integrates the acceleration over `dt` to calculate the velocity of `b`.
*/
void frIntegrateForBodyVelocity(frBody *b, float dt) {
    if (b == NULL || b->mtn.inverseMass <= 0.0f || dt <= 0.0f) return;

    b->mtn.velocity = frVector2Add(
        b->mtn.velocity,
        frVector2ScalarMultiply(b->mtn.force, b->mtn.inverseMass * dt)
    );

    b->mtn.angularVelocity += (b->mtn.torque * b->mtn.inverseInertia) * dt;
}

/* Integrates the velocity of `b` over `dt` to calculate the position of `b`. */
void frIntegrateForBodyPosition(frBody *b, float dt) {
    if (b == NULL || b->type == FR_BODY_STATIC || dt <= 0.0f) return;

    b->tx.position.x += b->mtn.velocity.x * dt;
    b->tx.position.y += b->mtn.velocity.y * dt;

    frSetBodyAngle(b, b->tx.angle + (b->mtn.angularVelocity * dt));

    b->aabb = frGetShapeAABB(b->shape, b->tx);
}

/* Private Functions ==================================================================== */

/* Computes the mass and the moment of inertia for `b`. */
static void frComputeBodyMass(frBody *b) {
    b->mtn.mass = b->mtn.inverseMass = 0.0f;
    b->mtn.inertia = b->mtn.inverseInertia = 0.0f;

    switch (b->type) {
        case FR_BODY_STATIC:
            b->mtn.velocity.x = b->mtn.velocity.y = b->mtn.angularVelocity = 0.0f;

            break;

        case FR_BODY_DYNAMIC:
            if (!(b->flags & FR_FLAG_INFINITE_MASS)) {
                b->mtn.mass = frGetShapeMass(b->shape);

                if (b->mtn.mass > 0.0f) b->mtn.inverseMass = 1.0f / b->mtn.mass;
            }

            if (!(b->flags & FR_FLAG_INFINITE_INERTIA)) {
                b->mtn.inertia = frGetShapeInertia(b->shape);
                
                if (b->mtn.inertia > 0.0f) b->mtn.inverseInertia = 1.0f / b->mtn.inertia;
            }

            break;

        default:
            break;
    }
}

/* Normalizes the `angle` to a range `[0, 2π]`. */
static FR_API_INLINE float frNormalizeAngle(float angle) {
    return angle - (TWO_PI * floorf(angle * INVERSE_TWO_PI));
}