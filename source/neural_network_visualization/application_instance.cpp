#include "application_instance.h"

#include "application_scene.h"

GAME_INSTANCE_IMPLEMENTAION(application_instance)

std::shared_ptr<gs::scene> application_instance::create_first_scene()
{
    return std::make_shared<application_scene>();
}

void application_instance::on_create()
{
}