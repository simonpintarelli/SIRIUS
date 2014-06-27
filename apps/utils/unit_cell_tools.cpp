#include <sirius.h>

using namespace sirius;

int main(int argn, char** argv)
{
    Platform::initialize(1);
    Global p;
    p.read_unit_cell_input();

    if (argn > 2)
    {
        int scell[3][3];

        std::string key(argv[1]);
        std::string value(argv[2]);

        if (key == "--supercell")
        {
            std::stringstream s(value);
            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++) s >> scell[i][j];
            }
            
            std::cout << std::endl;
            std::cout << "supercell vectors (lattice coordinates) : " << std::endl;
            for (int i = 0; i < 3; i++)
            {
                std::cout << "A" << i << " : ";
                for (int j = 0; j < 3; j++) std::cout << scell[i][j] << " ";
                std::cout << std::endl;
            }

            double scell_lattice_vectors[3][3];
            for (int i = 0; i < 3; i++)
            {
                for (int x = 0; x < 3; x++)
                {
                    scell_lattice_vectors[i][x] = scell[i][0] * p.unit_cell()->lattice_vectors(0, x) +
                                                  scell[i][1] * p.unit_cell()->lattice_vectors(1, x) +
                                                  scell[i][2] * p.unit_cell()->lattice_vectors(2, x);
                }
            }

            std::cout << "supercell vectors (Cartesian coordinates) : " << std::endl;
            for (int i = 0; i < 3; i++)
            {
                std::cout << "A" << i << " : ";
                for (int x = 0; x < 3; x++) std::cout << scell_lattice_vectors[i][x] << " ";
                std::cout << std::endl;
            }

            std::cout << "volume ratio : " << std::abs(matrix3d<int>(scell).det()) << std::endl;

            Global psc;
            psc.unit_cell()->set_lattice_vectors(scell_lattice_vectors[0], scell_lattice_vectors[1], scell_lattice_vectors[2]);
    
            for (int iat = 0; iat < (int)p.unit_cell_input_section_.labels_.size(); iat++)
            {
                std::string label = p.unit_cell_input_section_.labels_[iat];
                psc.unit_cell()->add_atom_type(label, p.unit_cell_input_section_.atom_files_[label], p.esm_type());
                for (int ia = 0; ia < (int)p.unit_cell_input_section_.coordinates_[iat].size(); ia++)
                {
                    vector3d<double> va(&p.unit_cell_input_section_.coordinates_[iat][ia][0]);

                    for (int i0 = -10; i0 <= 10; i0++)
                    {
                        for (int i1 = -10; i1 <= 10; i1++)
                        {
                            for (int i2 = -10; i2 <= 10; i2++)
                            {
                                vector3d<double> T(i0, i1, i2);
                                vector3d<double> vc = p.unit_cell()->get_cartesian_coordinates(va + T);
                                vector3d<double> vf = psc.unit_cell()->get_fractional_coordinates(vc);

                                auto vr = Utils::reduce_coordinates(vf);
                                bool add_atom = (psc.unit_cell()->atom_id_by_position(vr.first) == -1);
                                //==if (add_atom && iat == 2)
                                //=={
                                //==    double r = type_wrapper<double>::random();
                                //==    if (r < 0.99) add_atom = false;
                                //==}

                                if (add_atom) psc.unit_cell()->add_atom(label, &vr.first[0]);
                            }
                        }
                    }
                }
            }
            
            psc.unit_cell()->get_symmetry();
            psc.unit_cell()->print_info();
            psc.unit_cell()->write_json();
            psc.unit_cell()->write_cif();
        }
    }

    //p.unit_cell()->get_symmetry();
    //p.unit_cell()->print_info();

    Platform::finalize();

}