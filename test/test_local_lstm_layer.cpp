#include <cstring>
#include <vector>

#include "cuda_runtime.h"
#include "gtest/gtest.h"
#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/test/test_gradient_check_util.hpp"
#include "caffe/test/test_caffe_main.hpp"
#include "caffe/layers/local_lstm_layer.hpp"
#include "caffe/layers/lstm_unit_layer.hpp"


namespace caffe{
	template <typename Dtype>
	class LocalLSTMLayerTest{
	public:
		LocalLSTMLayerTest() : x_(new Blob<Dtype>()), cont_(new Blob<Dtype>()), 
			h_(new Blob<Dtype>()), c_(new Blob<Dtype>()){
			this->SetUp();
		}

		~LocalLSTMLayerTest(){ delete x_;  delete cont_; delete h_; delete c_; }

		void TestSetUp(){
			shared_ptr<Layer<Dtype>> layer(new LocalLSTMLayer<Dtype>(layer_param_));
			layer->SetUp(bottom_, top_);
			CHECK_EQ(top_[0]->shape(0), 5);
			CHECK_EQ(top_[0]->shape(1), 1);
			CHECK_EQ(top_[0]->shape(2), 3);
			CHECK_EQ(top_[1]->shape(0), 5);
			CHECK_EQ(top_[1]->shape(1), 1);
			CHECK_EQ(top_[1]->shape(2), 3);
		}

		void TestForward(Caffe::Brew mode){
			shared_ptr<Layer<Dtype>> layer(new LocalLSTMLayer<Dtype>(layer_param_));
			Caffe::set_mode(mode);
			layer->SetUp(bottom_, top_);
			layer->Forward(bottom_, top_);
			if (mode == Caffe::CPU){
				bottom_[0]->ToTxt("bottom_cpu");
				top_[0]->ToTxt("top_cpu");
			}
			else{
				bottom_[0]->ToTxt("bottom_gpu");
				top_[0]->ToTxt("top_gpu");
			}
		}


		void TestGradients(Caffe::Brew mode){
			// test LSTMUnitLayer first
//			LSTMUnitLayer<Dtype>* layer_unit = new LSTMUnitLayer<Dtype>(LayerParameter());
//			vector<int> c_shape{ 1, 1, 3 };
//			vector<int> x_shape{ 1, 1, 12 };
//			vector<int> cont_shape{ 1, 1 };
//			Blob<Dtype>* c = new Blob<Dtype>(c_shape);
//			Blob<Dtype>* x = new Blob<Dtype>(x_shape);
//			Blob<Dtype>* cont = new Blob<Dtype>(cont_shape);
//			Blob<Dtype>* c_top = new Blob<Dtype>();
//			Blob<Dtype>* h_top = new Blob<Dtype>();
//			FillerParameter fill_param;
//			fill_param.set_type("gaussian");
//			fill_param.set_std(1);
//			Filler<Dtype>* filler = GetFiller<Dtype>(fill_param);
//			filler->Fill(c);
//			filler->Fill(x);
//			Dtype* cont_data = cont->mutable_cpu_data();
//			cont_data[0] = 1;
//			const vector<Blob<Dtype>*> unit_bottom{ c, x , cont};
//			const vector<Blob<Dtype>*> unit_top{ c_top, h_top };
//			Caffe::set_mode(mode);
			GradientChecker<Dtype> checker(0.01, 0.001);
//			checker.CheckGradientExhaustive(layer_unit, unit_bottom, unit_top, 0);
//			checker.CheckGradientExhaustive(layer_unit, unit_bottom, unit_top, 1);
			// then LSTM Layer
			LocalLSTMLayer<Dtype> layer(layer_param_);
			layer.SetUp(bottom_, top_);
			layer.Forward(bottom_, top_);
			vector<bool> prop_down(2, true);
			prop_down[1] = false;
			layer.Backward(top_, prop_down, bottom_);
			CHECK_GT(top_.size(), 0) << "Exhaustive mode requires at least one top blob.";
			checker.CheckGradientExhaustive(&layer, bottom_, top_, 0);
		}


	protected:
		void SetUp(){
			vector<int> x_shape{5, 1, 2};
			x_->Reshape(x_shape);
			Dtype* x_data = x_->mutable_cpu_data();
			vector<int> cont_shape{ x_shape[0], x_shape[1] };
			cont_->Reshape(cont_shape);
			Dtype* cont_data = cont_->mutable_cpu_data();
			for (int c = 0; c < 5; ++c){
				if (c == 0 || c == 2){
					cont_data[c] = 0;
				}
				else{
					cont_data[c] = 1;
				}
			}
			for (int c = 0; c < 5; ++c){
				for (int j = 0; j < 3; j++){
					x_data[c * 3 + j] = c * 0.1 + j * 0.3;
				}
			}

			//set bottom && top
			bottom_.push_back(x_);
			bottom_.push_back(cont_);
			top_.push_back(h_);
			top_.push_back(c_);
			propagate_down_.resize(1, true);

			// set layer_param_
			layer_param_.mutable_inner_product_param()->set_num_output(3);
			layer_param_.mutable_inner_product_param()->mutable_weight_filler()->set_type("gaussian");
			layer_param_.mutable_inner_product_param()->mutable_weight_filler()->set_std(0.1);
			layer_param_.mutable_inner_product_param()->mutable_bias_filler()->set_type("constant");
			layer_param_.mutable_inner_product_param()->mutable_bias_filler()->set_value(0.);
			layer_param_.mutable_recurrent_param()->set_back_length(-1);
			layer_param_.mutable_recurrent_param()->set_local_lr(0.1);
			layer_param_.mutable_recurrent_param()->set_local_lr_decay(0.99);
			layer_param_.mutable_recurrent_param()->set_local_gradient_clip(10);
			layer_param_.mutable_recurrent_param()->set_local_bias_term(true);
			layer_param_.mutable_recurrent_param()->set_local_momentum(0.9);
			layer_param_.mutable_recurrent_param()->set_local_decay_type("L2");
			layer_param_.mutable_recurrent_param()->set_local_decay(0.95);
			ParamSpec* param = layer_param_.mutable_recurrent_param()->add_local_param();
			param->set_lr_mult(1);
			param->set_decay_mult(1);
			param = layer_param_.mutable_recurrent_param()->add_local_param();
			param->set_lr_mult(2);
			param->set_decay_mult(0);
			param = layer_param_.mutable_recurrent_param()->add_local_param();
			param->set_lr_mult(1);
			param->set_decay_mult(1);
			param = layer_param_.mutable_recurrent_param()->add_local_param();
			param->set_lr_mult(2);
			param->set_decay_mult(0);
		}

		Blob<Dtype>* x_;
		Blob<Dtype>* h_;
		Blob<Dtype>* c_;
		Blob<Dtype>* cont_;

		vector<Blob<Dtype>*> bottom_;
		vector<Blob<Dtype>*> top_;

		vector<bool> propagate_down_;

		LayerParameter layer_param_;
	};
}

int main(int argc, char** argv){
	::google::InitGoogleLogging(*argv);
	FLAGS_logtostderr = true;
	caffe::LocalLSTMLayerTest<float> test;
	test.TestSetUp();
	test.TestForward(caffe::Caffe::CPU);
	test.TestGradients(caffe::Caffe::CPU);
	test.TestForward(caffe::Caffe::GPU);
//	test.TestGradients(caffe::Caffe::GPU);
	return 0;
}