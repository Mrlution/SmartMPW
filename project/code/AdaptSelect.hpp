//
// @author   liyan
// @contact  lyan_dut@outlook.com
//
#ifndef SMARTMPW_ADAPTSELECT_HPP
#define SMARTMPW_ADAPTSELECT_HPP

#include <future>

#include "Instance.hpp"
#include "MpwBinPack.hpp"

using namespace mbp;

class AdaptSelect {

	/// ��ѡ��ȶ���
	struct CandidateWidth {
		coord_t value;
		int iter;
		unique_ptr<MpwBinPack> mbp_solver; // ��ָ�룬����������ɵĿ���
	};

public:

	AdaptSelect() = delete;

	AdaptSelect(const Environment &env, const Config &cfg) :
		_env(env), _cfg(cfg), _ins(env), _gen(_cfg.random_seed),
		_obj_area(numeric_limits<coord_t>::max()) {}

	void run() {

		_start = clock();

		//vector<coord_t> candidate_widths = cal_candidate_widths_on_interval();
		vector<coord_t> candidate_widths = cal_candidate_widths_on_sqrt();
		vector<CandidateWidth> cw_objs; cw_objs.reserve(candidate_widths.size());

		// ��֧��ʼ��iter=1
		// ���߳�
		for (coord_t bin_width : candidate_widths) {
			cw_objs.push_back({ bin_width, 1, unique_ptr<MpwBinPack>(
				new MpwBinPack(_ins.get_polygon_ptrs(), bin_width, INF, _gen)) });
			cw_objs.back().mbp_solver->random_local_search(1);
			check_cwobj(cw_objs.back());
		}
		// ���߳� ==> async
		//vector<future<void>> futures; futures.reserve(candidate_widths.size());
		//for (coord_t bin_width : candidate_widths) {
		//	cw_objs.push_back({ bin_width, 1, unique_ptr<MpwBinPack>(
		//		new MpwBinPack(_ins.get_polygon_ptrs(), bin_width, INF, _gen)) });
		//	futures.push_back(async(&MpwBinPack::random_local_search, cw_objs.back().mbp_solver.get(), 1));
		//	//futures.push_back(async([&]() { cw_objs.back().mbp_solver->random_local_search(1); }));
		//}
		//for (auto &f : futures) { f.wait(); }
		//for (auto &cw_obj : cw_objs) { check_cwobj(cw_obj); }

		// �������У�Խ�����ѡ�и���Խ��
		sort(cw_objs.begin(), cw_objs.end(), [](const CandidateWidth &lhs, const CandidateWidth &rhs) {
			return lhs.mbp_solver->get_obj_area() > rhs.mbp_solver->get_obj_area(); });

		// ��ʼ����ɢ���ʷֲ�
		vector<int> probs; probs.reserve(cw_objs.size());
		for (int i = 1; i <= cw_objs.size(); ++i) { probs.push_back(2 * i); }
		discrete_distribution<> discrete_dist(probs.begin(), probs.end());

		// �����Ż�
		int curr_iter = 0; _iteration = 0;
		while (static_cast<double>(clock() - _start) / CLOCKS_PER_SEC < min(_ins.get_polygon_num(), _cfg.ub_asa_time)
			&& curr_iter++ - _iteration < _cfg.ub_asa_iter) {
			CandidateWidth &picked_width = cw_objs[discrete_dist(_gen)];
			picked_width.iter = min(4 * picked_width.iter, _cfg.ub_rls_iter);
			picked_width.mbp_solver->set_bin_height(coord_t(floor(1.0 * _obj_area / picked_width.value)));
			picked_width.mbp_solver->random_local_search(picked_width.iter);
			_iteration = check_cwobj(picked_width) ? curr_iter : _iteration;
			sort(cw_objs.begin(), cw_objs.end(), [](const CandidateWidth &lhs, const CandidateWidth &rhs) {
				return lhs.mbp_solver->get_obj_area() > rhs.mbp_solver->get_obj_area(); });
		}
	}

	void record_sol(const string &sol_path) const {
		ofstream sol_file(sol_path);
		for (auto &dst_node : _dst) {
			sol_file << "In Polygon:" << endl;
			for (auto &point : *dst_node->in_points) { sol_file << "(" << point.x << "," << point.y << ")"; }
			dst_node->to_out_points();
			sol_file << endl << "Out Polygon:" << endl;
			for_each(dst_node->out_points.begin(), dst_node->out_points.end(),
				[&](point_t &point) { sol_file << "(" << point.x << "," << point.y << ")"; });
			sol_file << endl;
		}
	}

	void draw_sol(const string &html_path) const {
		utils_visualize_drawer::Drawer html_drawer(html_path, _cfg.ub_width, _cfg.ub_height);
		for (auto &dst_node : _dst) {
			string polygon_str;
			for_each(dst_node->out_points.begin(), dst_node->out_points.end(),
				[&](point_t &point) { polygon_str += to_string(point.x) + "," + to_string(point.y) + " "; });
			html_drawer.polygon(polygon_str);
		}
	}

#ifndef SUBMIT
	void draw_ins() const {
		ifstream ifs(_env.ins_html_path());
		if (ifs.good()) { return; }
		utils_visualize_drawer::Drawer html_drawer(_env.ins_html_path(), _cfg.ub_width, _cfg.ub_height);
		for (auto &src_node : _ins.get_polygon_ptrs()) {
			string polygon_str;
			for_each(src_node->in_points->begin(), src_node->in_points->end(),
				[&](const point_t &point) { polygon_str += to_string(point.x) + "," + to_string(point.y) + " "; });
			html_drawer.polygon(polygon_str);
		}
	}

	void record_log() const {
		ofstream log_file(_env.log_path(), ios::app);
		log_file.seekp(0, ios::end);
		if (log_file.tellp() <= 0) {
			log_file << "Instance,"
				"InsArea,ObjArea,FillRatio,"
				"Width,Height,WHRatio,"
				"Iteration,Duration,TotalDuration,RandomSeed" << endl;
		}
		log_file << _env.instance_name() << ","
			<< _ins.get_total_area() << "," << _obj_area << "," << _fill_ratio << ","
			<< _width << "," << _height << "," << _wh_ratio << ","
			<< _iteration << "," << _duration << ","
			<< static_cast<double>(clock() - _start) / CLOCKS_PER_SEC << "," << _cfg.random_seed << endl;
	}
#endif // !SUBMIT

private:
	/// ������[lb_width, ub_width]�ڣ��Ⱦ�����ɺ�ѡ���
	vector<coord_t> cal_candidate_widths_on_interval(coord_t interval = 1) {
		vector<coord_t> candidate_widths;
		coord_t min_width = 0, max_width = 0;
		for (auto &ptr : _ins.get_polygon_ptrs()) {
			min_width = max(min_width, ptr->max_length);
			max_width += ptr->max_length;
		}
		min_width = ceil(max(min_width, _cfg.lb_width));
		max_width = ceil(min(max_width, _cfg.ub_width));

		candidate_widths.reserve(max_width - min_width + 1);
		for (coord_t cw = min_width; cw <= max_width; cw += interval) {
			if (cw * _cfg.ub_height < _ins.get_total_area()) { continue; }
			candidate_widths.push_back(cw);
		}
		return candidate_widths;
	}

	/// ��ƽ�����Ƴ���ȣ�������֧��Ŀ
	vector<coord_t> cal_candidate_widths_on_sqrt(coord_t interval = 1) {
		vector<coord_t> candidate_widths;
		coord_t min_width = max(coord_t(floor(_cfg.lb_scale * sqrt(_ins.get_total_area()))), _cfg.lb_width);
		for_each(_ins.get_polygon_ptrs().begin(), _ins.get_polygon_ptrs().end(),
			[&](const polygon_ptr &ptr) { min_width = max(min_width, ptr->max_length); });
		coord_t max_width = max(min(coord_t(ceil(_cfg.ub_scale * sqrt(_ins.get_total_area()))), _cfg.ub_width), min_width);

		candidate_widths.reserve(max_width - min_width + 1);
		for (coord_t cw = min_width; cw <= max_width; cw += interval) {
			if (cw * _cfg.ub_height < _ins.get_total_area()) { continue; }
			candidate_widths.push_back(cw);
		}
		return candidate_widths;
	}

	/// ���cw_obj��RLS���
	bool check_cwobj(const CandidateWidth &cw_obj) {
		coord_t cw_height = cw_obj.mbp_solver->get_obj_area() / cw_obj.value;
		if (cw_height > _cfg.ub_height) { // ��߶ȳ����Ͻ磬���Ϸ�
			cw_obj.mbp_solver->set_obj_area(numeric_limits<coord_t>::max());
		}
		if (cw_height < _cfg.lb_height) { // ��߶Ȳ����½磬���½����
			cw_obj.mbp_solver->set_obj_area(cw_obj.value * _cfg.lb_height);
		}
		if (cw_obj.mbp_solver->get_obj_area() < _obj_area) {
			_obj_area = cw_obj.mbp_solver->get_obj_area();
			_fill_ratio = 1.0 * _ins.get_total_area() / _obj_area;
			_width = cw_obj.value;
			_height = cw_height;
			_wh_ratio = 1.0 * max(_width, _height) / min(_width, _height);
			_dst = cw_obj.mbp_solver->get_dst();
			_duration = static_cast<double>(clock() - _start) / CLOCKS_PER_SEC;
			return true;
		}
		return false;
	}

private:
	const Environment &_env;
	const Config &_cfg;

	const Instance _ins;
	default_random_engine _gen;
	clock_t _start;
	double _duration; // ���Ž����ʱ��
	int _iteration;   // ���Ž���ֵ�������

	coord_t _obj_area;
	double _fill_ratio;
	coord_t _width;
	coord_t _height;
	double _wh_ratio;
	vector<polygon_ptr> _dst;
};

#endif // SMARTMPW_ADAPTSELECT_HPP
