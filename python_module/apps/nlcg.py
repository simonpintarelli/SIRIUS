#!/usr/bin/env -S python -u -m mpi4py
from sirius.nlcg import run, validate_config
import argparse
from sirius import save_state
import yaml

if __name__ == '__main__':
    parser = argparse.ArgumentParser('')
    parser.add_argument('--sirius-config', '-s', default='sirius.json')
    parser.add_argument('--input', '-i', default='nlcg.yaml')
    parser.add_argument('--dump-on-error', '-e',
                        action='store_true', default=False)

    args = parser.parse_args()
    yaml_config = args.input
    ycfg = yaml.load(open(yaml_config, 'r'))
    # validate CG-config
    ycfg['CG'] = validate_config(ycfg['CG'])

    def callback(kset,
                 interval=ycfg['CG']['callback_interval'],
                 **kwargs):
        E = kwargs['E']

        def _callback(fn, it, **kwargs):
            if it % interval == 0:
                # kset.ctx().create_storage_file()
                # store_density_potential(E.density, E.potential)
                mag_mom = E.density.compute_atomic_mag_mom()
                save_state({'f': fn, 'ek': kset.e, 'mag_mom': mag_mom},
                           kset=kset, prefix='fn_%05d_' % it)
        return _callback

    def error_callback(kset,
                       **kwargs):
        E = kwargs['E']
        if parser.dump_on_error:
            def _callback(fn, it, **kwargs):
                # kset.ctx().create_storage_file()
                # store_density_potential(E.density, E.potential)
                mag_mom = E.density.compute_atomic_mag_mom()
                save_state({'f': fn, 'ek': kset.e, 'mag_mom': mag_mom,
                            'X': kwargs['X'], 'eta': kwargs['eta'],
                            'g_X': kwargs['g_X'], 'G_X': kwargs['G_X'],
                            'g_eta': kwargs['g_eta']},
                           kset=kset, prefix='fn_%05d_' % it)
        else:
            def _callback(*args, **kwargs):
                return None
        return _callback

    run(ycfg, args.sirius_config,
        callback=callback,
        error_callback=error_callback)
