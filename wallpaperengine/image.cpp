#include <irrlicht/irrlicht.h>
#include <fstream>

#include <wallpaperengine/fs/fileResolver.h>
#include <wallpaperengine/object3d.h>
#include <wallpaperengine/image.h>

#include <wallpaperengine/irrlicht.h>
#include <wallpaperengine/core.h>

namespace wp
{
    image::image (json json_data, wp::scene* scene) : object3d (object3d::Type::Type_Material, scene)
    {
        json::const_iterator visible_it = json_data.find ("visible");
        json::const_iterator file_it = json_data.find ("image");
        json::const_iterator origin_it = json_data.find ("origin");

        if (visible_it != json_data.end () && (*visible_it).is_boolean () == true)
        {
            this->m_visible = *visible_it;
        }

        // basic texture, main thing to assign first
        if (file_it != json_data.end () && (*file_it).is_string () == true)
        {
            this->m_file = wp::fs::resolver.resolve (*file_it);

            this->m_content = "";
            std::ifstream _in (this->m_file.c_str ());
            this->m_content.append (std::istreambuf_iterator<char> (_in), std::istreambuf_iterator<char> ());
            json content = json::parse (this->m_content);
            json::const_iterator it = content.find ("material");

            if (it != content.end () && (*it).is_string () == true)
            {
                irr::io::path materialfile = wp::fs::resolver.resolveOnWorkingDirectory ((*it));

                std::ifstream _in (materialfile.c_str ());
                std::string texturejson_content = "";
                texturejson_content.append (std::istreambuf_iterator<char> (_in), std::istreambuf_iterator<char> ());
                json materialcontent = json::parse (texturejson_content);

                // change working directory for the resolver to the materials folder
                this->m_resolver = wp::fs::resolver.clone ();
                this->m_resolver.changeWorkingDirectory(this->m_resolver.resolveOnWorkingDirectory ("materials"));

                // now try to read the texture if any
                json::const_iterator it = materialcontent.find ("passes");

                if (it != materialcontent.end () && (*it).is_array() == true)
                {
                    json::const_iterator passesCur = (*it).begin ();
                    json::const_iterator passesEnd = (*it).end ();

                    for (; passesCur != passesEnd; passesCur ++)
                    {
                        json::const_iterator texture = (*passesCur).find ("textures");

                        if (texture != (*passesCur).end () && (*texture).is_array () == true)
                        {
                            json::const_iterator texturesCur = (*texture).begin ();
                            json::const_iterator texturesEnd = (*texture).end ();

                            for (; texturesCur != texturesEnd; texturesCur ++)
                            {
                                irr::io::path texturePath = this->m_resolver.resolveOnWorkingDirectory ((*texturesCur));

                                this->m_textures.push_back (new wp::texture (texturePath));
                            }
                        }
                    }
                }
            }
        }

        // TODO: CHECK EFFECT PASSES HERE SO WE CAN TAKE IN ACCOUNT THE EXTRA TEXTURES FOR SHADERS, ETC

        if (origin_it != json_data.end ())
        {
            std::string origin = *origin_it;
            this->m_origin = wp::core::ato3vf (origin.c_str ());
        }
        else
        {
            // TOOD: CHANGE TO CENTER?
            this->m_origin = irr::core::vector3df (0.0f, 0.0f, 0.0f);
        }

        // initialize actual material properties
        irr::f32 xright = this->m_origin.X;
        irr::f32 xleft = -this->m_origin.X;
        irr::f32 ztop = this->m_origin.Y;
        irr::f32 zbottom = -this->m_origin.Y;
        irr::f32 z = this->m_scene->getCamera ()->getEye ().Z;

        m_vertices [0].Pos = irr::core::vector3df (xleft,   ztop,    z); // top left
        m_vertices [1].Pos = irr::core::vector3df (xright,  ztop,    z); // top right
        m_vertices [2].Pos = irr::core::vector3df (xright,  zbottom, z); // bottom right
        m_vertices [3].Pos = irr::core::vector3df (xleft,   zbottom, z); // bottom left

        m_vertices [0].TCoords = irr::core::vector2df (1.0f, 0.0f);
        m_vertices [1].TCoords = irr::core::vector2df (0.0f, 0.0f);
        m_vertices [2].TCoords = irr::core::vector2df (0.0f, 1.0f);
        m_vertices [3].TCoords = irr::core::vector2df (1.0f, 1.0f);

        m_vertices [0].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices [1].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices [2].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices [3].Color = irr::video::SColor (255, 255, 255, 255);

        // once textures are defined, prepare them to be displayed
        std::vector<wp::texture*>::const_iterator cur = this->m_textures.begin ();
        std::vector<wp::texture*>::const_iterator end = this->m_textures.end ();

        for (int i = 0; cur != end; cur ++, i ++)
        {
            this->getMaterial ().setTexture (i, (*cur)->getIrrTexture ());
        }
    }



    void image::setFlag(irr::video::E_MATERIAL_FLAG flag, bool newvalue)
    {
        this->getMaterial ().setFlag (flag, newvalue);
    }

    void image::setType(irr::video::E_MATERIAL_TYPE newType)
    {
        this->getMaterial ().MaterialType = newType;
    }

    irr::video::SMaterial& image::getMaterial ()
    {
        return this->m_material;
    }

    void image::render ()
    {
        uint16_t indices[] =
        {
                0, 1, 2, 3
        };

        wp::irrlicht::driver->setMaterial (this->m_material);
        wp::irrlicht::driver->drawVertexPrimitiveList (this->m_vertices, 4, indices, 1, irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT);
    }

}