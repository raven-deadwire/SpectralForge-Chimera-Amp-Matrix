import copy
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'Tools'))
from eq_tuner_parameter_contract import APPENDED_IDS, align_metadata, normalized_equal, rows

class AppendOnlyContracts(unittest.TestCase):
    def setUp(self):
        self.old = rows(Path(__file__).parent/'fixtures/eq-beta2/parameters.tsv')
        self.new = copy.deepcopy(self.old[:4967])
        self.new += [dict(index=str(4967+i), id=id, title='new', steps='0', default='0', flags='1')
                     for i,id in enumerate(APPENDED_IDS)]
        self.new += [dict(row, index=str(int(row['index'])+59)) for row in self.old[4967:]]

    def test_exact_append(self):
        self.assertEqual(len(align_metadata(self.old,self.new)),len(self.old))

    def test_old_id_order_range_default_flags_and_duplicate_rejected(self):
        for index,key,value in [(2,'id','999'),(5,'index','6'),(10,'steps','999'),
                                (4966,'default','.2'),(4966,'flags','0'),(5026,'id',self.new[0]['id'])]:
            with self.subTest(index=index,key=key):
                bad=copy.deepcopy(self.new);bad[index][key]=value
                with self.assertRaises(ValueError):align_metadata(self.old,bad)

    def test_missing_extra_or_wrong_new_id_rejected(self):
        for kind in ('missing','extra','wrong'):
            bad=copy.deepcopy(self.new)
            if kind=='missing':bad.pop()
            elif kind=='extra':bad.append(dict(bad[-1],index=str(len(bad)),id='777'))
            else:bad[4967]['id']='777'
            with self.assertRaises(ValueError):align_metadata(self.old,bad)

    def test_normalized_values_and_new_defaults_are_exact(self):
        with tempfile.TemporaryDirectory() as tmp:
            a,b=Path(tmp)/'a',Path(tmp)/'b';a.mkdir();b.mkdir()
            (a/'normalized.tsv').write_text('id\tvalue\n7\t0.5\n')
            base='id\tvalue\n7\t0.5\n'+''.join(f'{id}\t0\n' for id in APPENDED_IDS)
            (b/'normalized.tsv').write_text(base)
            (b/'parameters.tsv').write_text('id\tdefault\n'+''.join(f'{id}\t0\n' for id in APPENDED_IDS))
            normalized_equal(a,b)
            for bad in (base.replace('7\t0.5','7\t0.50000001'),base.replace(APPENDED_IDS[0]+'\t0',APPENDED_IDS[0]+'\t0.1'),base+'99\t0\n'):
                (b/'normalized.tsv').write_text(bad)
                with self.assertRaises(ValueError):normalized_equal(a,b)

if __name__=='__main__':unittest.main()
