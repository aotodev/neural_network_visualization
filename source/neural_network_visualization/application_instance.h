#pragma once

#include <gensou/core.h>


class application_instance : public gs::game_instance
{
public:
    virtual void on_create() override;

protected:
    virtual std::shared_ptr<gs::scene> create_first_scene() override;

};
