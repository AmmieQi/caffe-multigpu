#include <cstring>
#include <vector>

#include "cuda_runtime.h"
#include "gtest/gtest.h"
#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/test/test_gradient_check_util.hpp"
#include "caffe/test/test_caffe_main.hpp"
#include "caffe/layers/dec_lstm_layer.hpp"
#include "caffe/layers/dec_lstm_unit_layer.hpp"


namespace caffe{
	template <typename Dtype>
	class DLSTMLayerTest{
	public:
		DLSTMLayerTest() : c0_(new Blob<Dtype>()), h0_(new Blob<Dtype>()),
			x_(new Blob<Dtype>()), cont_(new Blob<Dtype>()), y_(new Blob<Dtype>()){
			this->SetUp();
		}

		~DLSTMLayerTest(){ delete x_; delete c0_; delete h0_; delete cont_; }

		void TestSetUp(){
			shared_ptr<Layer<Dtype>> layer(new DLSTMLayer<Dtype>(layer_param_));
			layer->SetUp(bottom_, top_);
			LOG(INFO) << top_[0]->shape_string();
			CHECK_EQ(top_[0]->shape(0), 5);
			CHECK_EQ(top_[0]->shape(1), 1);
			CHECK_EQ(top_[0]->shape(2), 4);
		}

		void TestForward(Caffe::Brew mode){
			shared_ptr<Layer<Dtype>> layer(new DLSTMLayer<Dtype>(layer_param_));
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
			// test DLSTMUnitLayer first
			DLSTMUnitLayer<Dtype>* layer_unit = new DLSTMUnitLayer<Dtype>(LayerParameter());
			vector<int> c_shape{ 1, 1, 3 };
			vector<int> x_shape{ 1, 1, 12 };
			Blob<Dtype>* c = new Blob<Dtype>(c_shape);
			Blob<Dtype>* x = new Blob<Dtype>(x_shape);
			Blob<Dtype>* c_top = new Blob<Dtype>();
			Blob<Dtype>* h_top = new Blob<Dtype>();
			FillerParameter fill_param;
			fill_param.set_type("gaussian");
			fill_param.set_std(1);
			Filler<Dtype>* filler = GetFiller<Dtype>(fill_param);
			filler->Fill(c);
			filler->Fill(x);
			const vector<Blob<Dtype>*> unit_bottom{ c, x };
			const vector<Blob<Dtype>*> unit_top{ c_top, h_top };
			Caffe::set_mode(mode);
			GradientChecker<Dtype> checker(0.01, 0.001);
			checker.CheckGradientExhaustive(layer_unit, unit_bottom, unit_top);
			// then DLSTM Layer
			DLSTMLayer<Dtype> layer(layer_param_);
			CHECK_GT(top_.size(), 0) << "Exhaustive mode requires at least one top blob.";
			checker.CheckGradientExhaustive(&layer, bottom_, top_, 0);
			checker.CheckGradientExhaustive(&layer, bottom_, top_, 1);
			checker.CheckGradientExhaustive(&layer, bottom_, top_, 3);
		}


	protected:
		void SetUp(){
			vector<int> c0_shape;
			c0_shape.push_back(2);
			c0_shape.push_back(1);
			c0_shape.push_back(2);
			c0_->Reshape(c0_shape);
			h0_->Reshape(c0_shape);
			Dtype* c0_data = c0_->mutable_cpu_data();
			Dtype* h0_data = h0_->mutable_cpu_data();
			vector<int> x_shape = c0_shape;
			x_shape[0] = 5;
			x_shape[2] = 3;
			x_->Reshape(x_shape);
			Dtype* x_data = x_->mutable_cpu_data();
			vector<int> cont_shape{ x_shape[0], x_shape[1] };
			cont_->Reshape(cont_shape);
			Dtype* cont_data = cont_->mutable_cpu_data();
			for (int c = 0; c < 2; ++c){
				for (int j = 0; j < 2; j++){
					c0_data[c * 2 + j] = j * 2 + c;
					h0_data[c * 2 + j] = c * 0.1;
				}
			}
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
			bottom_.push_back(c0_);
			bottom_.push_back(h0_);
			bottom_.push_back(cont_);
			bottom_.push_back(x_);
			top_.push_back(y_);
			propagate_down_.resize(3, true);

			// set layer_param_
			layer_param_.mutable_inner_product_param()->set_num_output(3);
			layer_param_.mutable_inner_product_param()->mutable_weight_filler()->set_type("gaussian");
			layer_param_.mutable_inner_product_param()->mutable_weight_filler()->set_std(0.1);
			layer_param_.mutable_inner_product_param()->mutable_bias_filler()->set_type("constant");
			layer_param_.mutable_inner_product_param()->mutable_bias_filler()->set_value(0.);
			layer_param_.mutable_recurrent_param()->set_conditional(true);
			layer_param_.mutable_recurrent_param()->set_output_dim(4);
			layer_param_.mutable_recurrent_param()->set_delay(true);
		}

		Blob<Dtype>* c0_;
		Blob<Dtype>* h0_;
		Blob<Dtype>* x_;
		Blob<Dtype>* y_;
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
	caffe::DLSTMLayerTest<float> test;
//	test.TestSetUp();
	test.TestForward(caffe::Caffe::CPU);
	test.TestGradients(caffe::Caffe::CPU);
//	test.TestForward(caffe::Caffe::GPU);
	test.TestGradients(caffe::Caffe::GPU);
	return 0;
}