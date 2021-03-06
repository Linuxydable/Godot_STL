/*************************************************************************/
/*  collision_object_bullet.cpp                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "collision_object_bullet.h"

#include <algorithm>

#include "area_bullet.h"
#include "bullet_physics_server.h"
#include "bullet_types_converter.h"
#include "bullet_utilities.h"
#include "shape_bullet.h"
#include "space_bullet.h"

#include <btBulletCollisionCommon.h>

/**
	@author AndreaCatania
*/

// We enable dynamic AABB tree so that we can actually perform a broadphase on bodies with compound collision shapes.
// This is crucial for the performance of kinematic bodies and for bodies with transforming shapes.
#define enableDynamicAabbTree true

CollisionObjectBullet::ShapeWrapper::~ShapeWrapper() {}

void CollisionObjectBullet::ShapeWrapper::set_transform(const Transform &p_transform) {
	G_TO_B(p_transform.get_basis().get_scale_abs(), scale);
	G_TO_B(p_transform, transform);
	UNSCALE_BT_BASIS(transform);
}

void CollisionObjectBullet::ShapeWrapper::set_transform(const btTransform &p_transform) {
	transform = p_transform;
}

btTransform CollisionObjectBullet::ShapeWrapper::get_adjusted_transform() const {
	if (shape->get_type() == PhysicsServer::SHAPE_HEIGHTMAP) {
		const HeightMapShapeBullet *hm_shape = (const HeightMapShapeBullet *)shape; // should be safe to cast now
		btTransform adjusted_transform;

		// Bullet centers our heightmap:
		// https://github.com/bulletphysics/bullet3/blob/master/src/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h#L33
		// This is really counter intuitive so we're adjusting for it

		adjusted_transform.setIdentity();
		adjusted_transform.setOrigin(btVector3(0.0, hm_shape->min_height + ((hm_shape->max_height - hm_shape->min_height) * 0.5), 0.0));
		adjusted_transform *= transform;

		return adjusted_transform;
	} else {
		return transform;
	}
}

void CollisionObjectBullet::ShapeWrapper::claim_bt_shape(const btVector3 &body_scale) {
	if (!bt_shape) {
		if (active)
			bt_shape = shape->create_bt_shape(scale * body_scale);
		else
			bt_shape = ShapeBullet::create_shape_empty();
	}
}

CollisionObjectBullet::CollisionObjectBullet(Type p_type) :
		RIDBullet(),
		type(p_type),
		instance_id(0),
		collisionLayer(0),
		collisionMask(0),
		collisionsEnabled(true),
		m_isStatic(false),
		ray_pickable(false),
		bt_collision_object(NULL),
		body_scale(1., 1., 1.),
		force_shape_reset(false),
		space(NULL),
		isTransformChanged(false) {}

CollisionObjectBullet::~CollisionObjectBullet() {
	// Remove all overlapping, notify is not required since godot take care of it
	for(auto it = areasOverlapped.rbegin(); it != areasOverlapped.rend(); ++it ){
		(*it)->remove_overlap(this, /*Notify*/ false);
	}

	destroyBulletCollisionObject();
}

bool equal(real_t first, real_t second) {
	return Math::abs(first - second) <= 0.001f;
}

void CollisionObjectBullet::set_body_scale(const Vector3 &p_new_scale) {
	if (!equal(p_new_scale[0], body_scale[0]) || !equal(p_new_scale[1], body_scale[1]) || !equal(p_new_scale[2], body_scale[2])) {
		body_scale = p_new_scale;
		body_scale_changed();
	}
}

btVector3 CollisionObjectBullet::get_bt_body_scale() const {
	btVector3 s;
	G_TO_B(body_scale, s);
	return s;
}

void CollisionObjectBullet::body_scale_changed() {
	force_shape_reset = true;
}

void CollisionObjectBullet::destroyBulletCollisionObject() {
	bulletdelete(bt_collision_object);
}

void CollisionObjectBullet::setupBulletCollisionObject(btCollisionObject *p_collisionObject) {
	bt_collision_object = p_collisionObject;
	bt_collision_object->setUserPointer(this);
	bt_collision_object->setUserIndex(type);
	// Force the enabling of collision and avoid problems
	set_collision_enabled(collisionsEnabled);
	p_collisionObject->setCollisionFlags(p_collisionObject->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
}

void CollisionObjectBullet::add_collision_exception(const CollisionObjectBullet *p_ignoreCollisionObject) {
	exceptions.insert(p_ignoreCollisionObject->get_self());
	if (!bt_collision_object)
		return;
	bt_collision_object->setIgnoreCollisionCheck(p_ignoreCollisionObject->bt_collision_object, true);
	if (space)
		space->get_broadphase()->getOverlappingPairCache()->cleanProxyFromPairs(bt_collision_object->getBroadphaseHandle(), space->get_dispatcher());
}

void CollisionObjectBullet::remove_collision_exception(const CollisionObjectBullet *p_ignoreCollisionObject) {
	exceptions.erase(p_ignoreCollisionObject->get_self());
	bt_collision_object->setIgnoreCollisionCheck(p_ignoreCollisionObject->bt_collision_object, false);
	if (space)
		space->get_broadphase()->getOverlappingPairCache()->cleanProxyFromPairs(bt_collision_object->getBroadphaseHandle(), space->get_dispatcher());
}

bool CollisionObjectBullet::has_collision_exception(const CollisionObjectBullet *p_otherCollisionObject) const {
	return !bt_collision_object->checkCollideWith(p_otherCollisionObject->bt_collision_object);
}

void CollisionObjectBullet::set_collision_enabled(bool p_enabled) {
	collisionsEnabled = p_enabled;
	if (collisionsEnabled) {
		bt_collision_object->setCollisionFlags(bt_collision_object->getCollisionFlags() & (~btCollisionObject::CF_NO_CONTACT_RESPONSE));
	} else {
		bt_collision_object->setCollisionFlags(bt_collision_object->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	}
}

bool CollisionObjectBullet::is_collisions_response_enabled() {
	return collisionsEnabled;
}

void CollisionObjectBullet::notify_new_overlap(AreaBullet *p_area) {
	areasOverlapped.push_back(p_area);
}

void CollisionObjectBullet::on_exit_area(AreaBullet *p_area) {
	auto it = std::find(areasOverlapped.begin(), areasOverlapped.end(), p_area);

	if(it != areasOverlapped.end()){
		areasOverlapped.erase(it);
	}
}

void CollisionObjectBullet::set_godot_object_flags(int flags) {
	bt_collision_object->setUserIndex2(flags);
}

int CollisionObjectBullet::get_godot_object_flags() const {
	return bt_collision_object->getUserIndex2();
}

void CollisionObjectBullet::set_transform(const Transform &p_global_transform) {

	set_body_scale(p_global_transform.basis.get_scale_abs());

	btTransform bt_transform;
	G_TO_B(p_global_transform, bt_transform);
	UNSCALE_BT_BASIS(bt_transform);

	set_transform__bullet(bt_transform);
}

Transform CollisionObjectBullet::get_transform() const {
	Transform t;
	B_TO_G(get_transform__bullet(), t);
	t.basis.scale(body_scale);
	return t;
}

void CollisionObjectBullet::set_transform__bullet(const btTransform &p_global_transform) {
	bt_collision_object->setWorldTransform(p_global_transform);
	notify_transform_changed();
}

const btTransform &CollisionObjectBullet::get_transform__bullet() const {
	return bt_collision_object->getWorldTransform();
}

void CollisionObjectBullet::notify_transform_changed() {
	isTransformChanged = true;
}

RigidCollisionObjectBullet::RigidCollisionObjectBullet(Type p_type) :
		CollisionObjectBullet(p_type),
		mainShape(NULL) {
}

RigidCollisionObjectBullet::~RigidCollisionObjectBullet() {
	remove_all_shapes(true, true);
	if (mainShape && mainShape->isCompound()) {
		bulletdelete(mainShape);
	}
}

void RigidCollisionObjectBullet::add_shape(ShapeBullet *p_shape, const Transform &p_transform, bool p_disabled) {
	shapes.push_back(ShapeWrapper(p_shape, p_transform, !p_disabled));
	p_shape->add_owner(this);
	reload_shapes();
}

void RigidCollisionObjectBullet::set_shape(int p_index, ShapeBullet *p_shape) {
	ShapeWrapper &shp = shapes[p_index];
	shp.shape->remove_owner(this);
	p_shape->add_owner(this);
	shp.shape = p_shape;
	reload_shapes();
}

int RigidCollisionObjectBullet::get_shape_count() const {
	return shapes.size();
}

ShapeBullet *RigidCollisionObjectBullet::get_shape(int p_index) const {
	return shapes[p_index].shape;
}

btCollisionShape *RigidCollisionObjectBullet::get_bt_shape(int p_index) const {
	return shapes[p_index].bt_shape;
}

int RigidCollisionObjectBullet::find_shape(ShapeBullet *p_shape) const {
	auto it = std::find_if(shapes.begin(), shapes.end(),
		[&](const ShapeWrapper& shapeW){
			if(shapeW.shape == p_shape){
				return true;
			}
			return false;
		}
	);

	if(it != shapes.end() ){
		return std::distance(shapes.begin(), it);
	}
	return -1;
}

void RigidCollisionObjectBullet::internal_shape_destroy_0(ShapeWrapper& shapeW, const bool& p_permanentlyFromThisBody){
	shapeW.shape->remove_owner(this, p_permanentlyFromThisBody);

	if (shapeW.bt_shape == mainShape) {
		mainShape = NULL;
	}

	delete shapeW.bt_shape;

	shapeW.bt_shape = NULL;
}

void RigidCollisionObjectBullet::remove_shape_full(ShapeBullet *p_shape) {
	// Remove the shape, all the times it appears
	// Reverse order required for delete.
	shapes.erase(
		std::remove_if(shapes.begin(), shapes.end(),
			[&](ShapeWrapper& shapeW){
				if(shapeW.shape == p_shape){
					internal_shape_destroy_0(shapeW);
					return true;
				}
				return false;
			}
		)
	,shapes.end() );

	reload_shapes();
}

void RigidCollisionObjectBullet::internal_shape_destroy(int p_index, bool p_permanentlyFromThisBody){
	internal_shape_destroy_0(shapes[p_index], p_permanentlyFromThisBody);
}

void RigidCollisionObjectBullet::remove_shape_full(int p_index) {
	ERR_FAIL_INDEX(p_index, get_shape_count());

	internal_shape_destroy(p_index);

	shapes.erase(shapes.begin()+p_index);

	reload_shapes();
}

void RigidCollisionObjectBullet::remove_all_shapes(bool p_permanentlyFromThisBody, bool p_force_not_reload) {
	// Reverse order required for delete.
	for(auto&& shapeW : shapes){
		internal_shape_destroy_0(shapeW, p_permanentlyFromThisBody);
	}

	shapes.clear();

	if (!p_force_not_reload)
		reload_shapes();
}

// need_update : use const ShapeWrapper& shape, not int p_index
void RigidCollisionObjectBullet::set_shape_transform(int p_index, const Transform &p_transform) {
	ERR_FAIL_INDEX(p_index, get_shape_count());

	shapes[p_index].set_transform(p_transform);

	shape_changed(p_index);
}

// need_update : use const ShapeWrapper& shape, not int p_index
const btTransform &RigidCollisionObjectBullet::get_bt_shape_transform(int p_index) const {
	return shapes[p_index].transform;
}

// need_update : use const ShapeWrapper& shape, not int p_index
Transform RigidCollisionObjectBullet::get_shape_transform(int p_index) const {
	Transform trs;
	B_TO_G(shapes[p_index].transform, trs);
	return trs;
}

// need_update : use const ShapeWrapper& shape, not int p_index
void RigidCollisionObjectBullet::set_shape_disabled(int p_index, bool p_disabled) {
	if (shapes[p_index].active != p_disabled)
		return;

	shapes[p_index].active = !p_disabled;

	shape_changed(p_index);
}

// need_update : use const ShapeWrapper& shape, not int p_index
bool RigidCollisionObjectBullet::is_shape_disabled(int p_index) {
	return !shapes[p_index].active;
}

// need_update : use const ShapeWrapper& shape, not int p_index
void RigidCollisionObjectBullet::shape_changed(int p_shape_index) {
	if (shapes[p_shape_index].bt_shape == mainShape) {
		mainShape = NULL;
	}

	bulletdelete(shapes[p_shape_index].bt_shape);

	reload_shapes();
}

void RigidCollisionObjectBullet::reload_shapes() {
	if (mainShape && mainShape->isCompound()) {
		// Destroy compound
		bulletdelete(mainShape);
	}

	mainShape = NULL;

	// Reset shape if required
	if (force_shape_reset) {
		for(auto&& shapeW : shapes){
			bulletdelete(shapeW.bt_shape);
		}

		force_shape_reset = false;
	}

	const btVector3 body_scale(get_bt_body_scale());

	// Try to optimize by not using compound
	if (shapes.size() == 1){
		btTransform transform = shapes[0].get_adjusted_transform();

		if( transform.getOrigin().isZero() && transform.getBasis() == transform.getBasis().getIdentity() ){
			shapes[0].claim_bt_shape(body_scale);

			mainShape = shapes[0].bt_shape;

			main_shape_changed();
			return;
		}
	}

	// Optimization not possible use a compound shape
	btCompoundShape *compoundShape = bulletnew( btCompoundShape( enableDynamicAabbTree, shapes.size() ) );

	for(auto&& shapeW : shapes){
		shapeW.claim_bt_shape(body_scale);

		btTransform scaled_shape_transform( shapeW.get_adjusted_transform() );

		scaled_shape_transform.getOrigin() *= body_scale;

		compoundShape->addChildShape(scaled_shape_transform, shapeW.bt_shape);
	}

	compoundShape->recalculateLocalAabb();

	mainShape = compoundShape;

	main_shape_changed();
}

void RigidCollisionObjectBullet::body_scale_changed() {
	CollisionObjectBullet::body_scale_changed();
	reload_shapes();
}
