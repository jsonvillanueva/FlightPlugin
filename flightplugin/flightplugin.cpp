#define LINMATH_H //Conflicts with linmath.h if we done declare this here

#include "flightplugin.h"
#include "bakkesmod\wrappers\ArrayWrapper.h"
#include "bakkesmod\wrappers\GameObject/CarWrapper.h"
#include "bakkesmod\wrappers\CVarManagerWrapper.h"
#include "bakkesmod\wrappers\CVarWrapper.h"
#include "bakkesmod\wrappers\Engine\ActorWrapper.h"
#include "bakkesmod\wrappers\GameEvent\ServerWrapper.h"
#include "bakkesmod\wrappers\GameObject\BallWrapper.h"
#include "bakkesmod\wrappers\GameObject\CarComponent\PrimitiveComponentWrapper.h"
#include "bakkesmod\wrappers\GameObject\CarComponent\VehicleSimWrapper.h"
#include "bakkesmod\wrappers\GameObject\CarWrapper.h"
#include "bakkesmod\wrappers\GameObject\RBActorWrapper.h"
#include "bakkesmod\wrappers\gamewrapper.h"
#include "bakkesmod\wrappers\wrapperstructs.h"
#include "bakkesmod\wrappers\GameEvent\TutorialWrapper.h"
#include "bakkesmod/wrappers/arraywrapper.h"
#include "utils\parser.h"


BAKKESMOD_PLUGIN(flightplugin, "Flight plugin", "1.0.0", PLUGINTYPE_FREEPLAY)

using namespace sp;

void flightplugin::onLoad()
{
	gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput", bind(&flightplugin::OnSetInput, this));


	liftp = make_shared<float>(0.0385);
	cvarManager->registerCvar("flight_lift", "0.0025", "Lift Power", true, true, 0, true, 1, true).bindTo(liftp);

	dragp = make_shared<float>(0.1);
	cvarManager->registerCvar("flight_drag", "0.1", "You flyin thru mud", true, true, 0, true, 1).bindTo(dragp);

	logger.cvarManager = this->cvarManager;
	cmdManager.cvarManager = this->cvarManager;
	cmdManager.gameWrapper = this->gameWrapper;

	cmdManager.addCommands();

	painter.gameWrapper = this->gameWrapper;
	painter.cvarManager = this->cvarManager;

	painter.initDrawables();

	cvarManager->registerNotifier("flight_enabled", [this](std::vector<string> params) {
		if (!gameWrapper->IsInFreeplay())
		{
			cvarManager->log("You need to be in freeplay to use this plugin.");
			return;
		}
		this->flight_enabled = true;
		this->OnSetInput();
		cvarManager->log("Get that air!");
		}, "Starts the Flight mode.", PERMISSION_FREEPLAY);
}
void flightplugin::onUnLoad()
{
	flight_enabled = false;
	return;
}
Vector flightplugin::reflect_v1_on_v2(Vector v1, Vector v2)
{ // take from: https://en.wikipedia.org/wiki/Specular_reflection
	Vector incident = v1.clone();
	Vector norm = v2.clone();
	Vector i_unit = incident.normalize();
	Vector n_unit = norm.normalize();
	float norm_dot = Vector::dot(i_unit, n_unit);
	if (norm_dot < 0) // Change the surface normal direction since car's velcotiy is hitting bottom/left/back face of car
	{
		n_unit = n_unit*-1;
	}
	float dot = Vector::dot(i_unit, n_unit);
	dot *= 2;
	Vector change = norm * dot;
	Vector reflect = change - incident; // The specularly reflected air direction
	return reflect; // Car moves in opposite direction of air
}
void flightplugin::OnSetInput()
{
	if (flight_enabled == false) //stops the plugin if flight_enabled isnt true
	{
		return;
	}

	/* definitions */
	auto car = gameWrapper->GetGameEventAsServer().GetGameCar();
	RBState rbstate = car.GetRBState();

	Vector loc = rbstate.Location;
	Vector lin = rbstate.LinearVelocity;
	Quat quat = rbstate.Quaternion;
	Vector ang = rbstate.AngularVelocity;
	auto horVel = Vector(lin.X, lin.Y, 0);
	Vector up = quatToUp(quat);
	Vector right = quatToRight(quat);
	Vector fwd = quatToFwd(quat);
	auto linLocalFwd = Vector::dot(lin, fwd);
	auto linLocalRight = Vector::dot(lin, right);
	auto linLocalUp = Vector::dot(lin, up);
	Vector linLocal = Vector(linLocalFwd, linLocalRight, linLocalUp);
	auto lonSpeed = Vector::dot(horVel, fwd);
	auto latSpeed = Vector::dot(horVel, right);



	// Lift 
	/* float lift = (*liftp) * lonSpeed;
	Vector liftd = { 0, 0, lift };
	Vector lifted = (lin, up) * lift;
	car.AddVelocity(liftd);
	*/

	// Drag
	/*	float drag = ((*dragp * lonSpeed) * -1);
		Vector dragp = {0, 0, drag};
		car.AddVelocity(dragp); */
}