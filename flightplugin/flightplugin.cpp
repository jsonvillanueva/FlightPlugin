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

	enabled = make_shared<bool>(false);
	rho = make_shared<float>(0.f);
	length = make_shared<float>(1.f);
	width = make_shared<float>(1.f);
	height = make_shared<float>(1.f);

	gameWrapper->HookEvent("Function TAGame.GameEvent_Tutorial_TA.OnInit", bind(&flightplugin::OnFreeplayLoad, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Tutorial_TA.Destroyed", bind(&flightplugin::OnFreeplayDestroy, this, std::placeholders::_1));

	cvarManager->registerCvar("flight_enabled", "0", "Enables/disable flight lift functionality", true, true, 0.f, true, 1.f)
		.addOnValueChanged(std::bind(&flightplugin::OnEnabledChanged, this, std::placeholders::_1, std::placeholders::_2));
	cvarManager->getCvar("flight_enabled").bindTo(enabled);
	cvarManager->registerCvar("air_density", "0", "Air Density", true, true, 0.f, true, 1.f, true).bindTo(rho);
	cvarManager->registerCvar("car_length", "1", "Car Length", true, true, 0.f, true, 10.f, true).bindTo(length);
	cvarManager->registerCvar("car_width", "1", "Car Width", true, true, 0.f, true, 10.f, true).bindTo(width);
	cvarManager->registerCvar("car_height", "1", "Car Height", true, true, 0.f, true, 10.f, true).bindTo(height);

	logger.cvarManager = this->cvarManager;
	cmdManager.cvarManager = this->cvarManager;
	cmdManager.gameWrapper = this->gameWrapper;

	cmdManager.addCommands();

	painter.gameWrapper = this->gameWrapper;
	painter.cvarManager = this->cvarManager;

	painter.initDrawables();
}
void flightplugin::onUnload()
{
}
void flightplugin::OnFreeplayLoad(std::string eventName)
{
	if (*enabled)
		gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
			bind(&flightplugin::OnSetInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}
void flightplugin::OnFreeplayDestroy(std::string eventName)
{
	gameWrapper->UnhookEvent("Function TAGame.RBActor_TA.PreAsyncTick");
}
void flightplugin::OnEnabledChanged(std::string oldValue, CVarWrapper cvar)
{
	if (cvar.getBoolValue() && gameWrapper->IsInFreeplay())
	{
		gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
			bind(&flightplugin::OnSetInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	else
	{
		cvarManager->log("Flight disabled");
		gameWrapper->UnhookEvent("Function TAGame.RBActor_TA.PreAsyncTick");
	}
}
Vector flightplugin::reflect_v1_on_v2(Vector incident, Vector n_unit)
{ // taken from: https://en.wikipedia.org/wiki/Specular_reflection
	float norm_dot = Vector::dot(incident, n_unit);
	if (norm_dot < 0) // Change the surface normal direction as air flux is hitting bottom/left/back face of car
	{
		n_unit = n_unit * -1;
		norm_dot *= -1.0;
	}
	norm_dot *= 2.0;
	Vector change = n_unit * norm_dot;
	Vector r_unit = change - incident;
	cvarManager->log("Norm_unit: " + sp::vector_to_string(n_unit, 5) + " " + sp::to_string(norm_dot));
	return r_unit; // Return unit vector of direction air bounces off car
}
void flightplugin::OnSetInput(CarWrapper cw, void * params, string funcName)
{
	if (gameWrapper->IsInFreeplay() && *enabled)
	{
		/* definitions */
		auto car = gameWrapper->GetGameEventAsServer().GetGameCar();
		RBState rbstate = car.GetRBState();

		Vector loc = rbstate.Location;
		Vector lin = rbstate.LinearVelocity; // Car velocity with respect to world
		float speed = lin.magnitude();
		Quat quat = rbstate.Quaternion;
		// The following vectors describe the basis of the car (the 3 sides of the car)
		Vector right = quatToRight(quat);
		Vector up = quatToUp(quat);
		Vector fwd = quatToFwd(quat);

		// Momentum vectors for air bouncing off car
		Vector deflect_right = reflect_v1_on_v2(lin, right); // Simulate air bouncing off car left/right
		Vector deflect_up = reflect_v1_on_v2(lin, up); // Simulate air bouncing off car roof/bottom
		Vector deflect_fwd = reflect_v1_on_v2(lin, fwd); // Simulate air bouncing off car front/back
		cvarManager->log("Right: " + sp::vector_to_string(deflect_right));
		cvarManager->log("Up: " + sp::vector_to_string(deflect_up));
		cvarManager->log("Fwd: " + sp::vector_to_string(deflect_fwd));

		// Resultant vector for car
		Vector res_right = (deflect_right + lin)/(-2.f);
		Vector res_up = (deflect_up + lin)/(-2.f);
		Vector res_fwd = (deflect_fwd + lin)/(-2.f);
		float coef = (*rho); // Apply reduction in speed
		cvarManager->log("True speed: " + sp::to_string(speed, 4));
		cvarManager->log("Air density: "+sp::to_string(coef, 5));
		cvarManager->log("L/W/H: " + sp::to_string(*length, 5) + "," + sp::to_string(*width, 5)+ "," + sp::to_string(*height,5));
		Vector extent = car.GetLocalCollisionExtent();

		// Flux calculation
		float w = extent.Y * (*width);
		float h = extent.Z * (*height);
		float l = extent.X * (*length);
		float roof_area = w * l;
		float door_area = l * h;
		float bumper_area = w * h;
		float total_area = roof_area + door_area + bumper_area;
		float roof = roof_area / total_area;
		float door = door_area / total_area;
		float bumper = bumper_area / total_area;
		float flux_r = abs(Vector::dot(deflect_right.norm(), right) * door * coef);
		float flux_u = abs(Vector::dot(deflect_up.norm(), up) * roof * coef);
		float flux_f = abs(Vector::dot(deflect_fwd.norm(), fwd) * bumper * coef);
		cvarManager->log(sp::to_string(flux_r, 5) + "   " + sp::to_string(flux_u, 5) + "   " + sp::to_string(flux_f, 5));

		// Modify resultant vector by flux
		res_right = res_right * flux_r;
		res_up = res_up * flux_u;
		res_fwd = res_fwd * flux_f;
		Vector result = res_up + res_fwd + res_right;
		cvarManager->log("Rscaled: " + sp::vector_to_string(res_right, 5));
		cvarManager->log("Uscaled: " + sp::vector_to_string(res_up, 5));
		cvarManager->log("Fscaled: " + sp::vector_to_string(res_fwd, 5));
		cvarManager->log("Result: " + sp::vector_to_string(result, 5));
		car.AddVelocity(result*1.f);
	}
}